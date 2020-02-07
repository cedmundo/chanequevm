#include "cvm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline retcode vm_run_step(struct vm *vm) {
  assert(vm != NULL);

  if (vm->halted) {
    printf("error: halted vm\n");
    return ERROR;
  }

  int64_t aux = 0;
  int64_t left = 0;
  int64_t right = 0;
  uint8_t *udata = NULL;

  uint8_t *curpos = vm->code + vm->code_offset;
  if (curpos > (vm->code + vm->code_size - 4)) {
    printf("error: no more instructions to read\n");
    return ERROR;
  }
  vm->code_offset += 4;

  uint32_t step = decode_step(curpos);
  uint8_t opcode = decode_opcode(step);
  uint8_t arg0 = decode_arg0(step);
  uint16_t arg1 = decode_arg1(step);

#ifdef CVM_PSTEP
  printf("feed: %02d %02d %02d %02d\n", curpos[0], curpos[1], curpos[2],
         curpos[3]);
  printf("step: opcode=%d, arg0=%d, arg1=%d\n", opcode, arg0, arg1);
#endif

  if ((opcode >= ADD && opcode <= GE) || opcode == BULK) {
    if (stack_pop(&vm->main, &right) == ERROR) {
      printf("error: missing right parameter, opc: %d, addr: %p\n", opcode,
             curpos);
      return ERROR;
    }

    if (stack_pop(&vm->main, &left) == ERROR) {
      printf("error: missing left parameter, opc: %d, addr: %p\n", opcode,
             curpos);
      return ERROR;
    }
  } else if (opcode >= NOT && opcode <= JZ) {
    if (stack_pop(&vm->main, &left) == ERROR) {
      printf("error: missing parameter, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
  }

  if (opcode >= BULK && opcode <= INSM) {
    if (stack_pop(&vm->mem, &aux) == ERROR) {
      printf("error: no allocated memory, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    udata = (uint8_t *)aux;
  }

  switch ((enum opcode)opcode) {
  case NOP:
    break;
  case HALT:
    vm->halted = 1;
    printf("vm has been halted\n");
    break;
  case CLEAR_STACK:
    while (stack_pop(&vm->main, &aux) != ERROR)
      ;
    break;
  case PRINT_STATE:
    printf("code section: %p, size: %ld, offset: %ld\n", vm->code,
           vm->code_size, vm->code_offset);
    printf("main stack:\n");
    stack_print(&vm->main);
    printf("memory stack:\n");
    stack_print(&vm->mem);
    printf("call stack:\n");
    stack_print(&vm->call);
    break;
  case PUSH:
    assert(arg0 == 0x00);
    aux = arg1;
    break;
  case POP:
    stack_pop(&vm->main, &aux);
    break;
  case ADD:
    aux = left + right;
    break;
  case SUB:
    aux = left - right;
    break;
  case MUL:
    aux = left * right;
    break;
  case DIV:
    if (right == 0) {
      printf("error: divide by zero, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    aux = left / right;
    break;
  case MOD:
    if (right == 0) {
      printf("error: modulo by zero, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    aux = left % right;
    break;
  case AND:
    aux = left & right;
    break;
  case OR:
    aux = left | right;
    break;
  case XOR:
    aux = left ^ right;
    break;
  case NEQ:
    aux = left != right;
    break;
  case EQ:
    aux = left == right;
    break;
  case LT:
    aux = left < right;
    break;
  case LE:
    aux = left <= right;
    break;
  case GT:
    aux = left > right;
    break;
  case GE:
    aux = left >= right;
    break;
  case NOT:
    aux = ~left;
    break;
  case JNZ:
    if (left != 0) {
      vm_jmp(vm, arg1);
    }
    stack_push(&vm->main, left);
    break;
  case JZ:
    if (left == 0) {
      vm_jmp(vm, arg1);
    }
    stack_push(&vm->main, left);
    break;
  case JMP:
    vm_jmp(vm, arg1);
    break;
  case RESV:
    udata = malloc(sizeof(uint8_t) * arg1);
    memset(udata, 0L, sizeof(uint8_t) * arg1);
    if (stack_push(&vm->mem, (int64_t)udata) == ERROR) {
      free(udata);
      printf("error: stack overflow on memory stack, opc: %d, addr: %p\n",
             opcode, curpos);
      return ERROR;
    }
    break;
  case FREE:
    if (stack_pop(&vm->mem, &aux) == ERROR) {
      printf("error: no allocated memory, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
    free((uint8_t *)aux);
    break;
  case BULK:
    if (((size_t)left) >= vm->code_size) {
      printf(
          "error: trying to copy outside of code segment, opc: %d, addr: %p\n",
          opcode, curpos);
      return ERROR;
    }

    if (udata == NULL) {
      printf(
          "error: trying to do a copy into a null pointer,  opc: %d, addr: %p",
          opcode, curpos);
      return ERROR;
    }

    // FIXME: SEVERE: Cannot probe that copied memory is smaller than target
    // memory slot
    memcpy(udata, vm->code + left, right);
    break;
  case LOAD: // LOAD A
    if (udata == NULL) {
      printf("error: trying to do a copy from a null pointer,  opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }

    // udata is already fill using aux value
    left = arg1;
    right = *(udata + left);
    if (stack_push(&vm->main, right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
    break;
  case STORE:
    if (udata == NULL) {
      printf("error: trying to do a copy from a null pointer,  opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }

    // udata is already fill using aux value
    left = arg1;
    if (stack_pop(&vm->main, &right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    *(udata + left) = right;
    break;
  case INSM:
    left = arg1;
    if (udata == NULL) {
      printf("error: trying to do a copy from a null pointer,  opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }

    printf("inspect: %p (%p + %ld) = %ld\n", (udata + left), udata, left,
           *(int64_t *)(udata + left));
    break;
  case CALL:
    if (stack_push(&vm->call, vm->code_offset) == ERROR) {
      printf("error: cannot save current address to do a call, opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }
    if (arg0 == 0x00) {
      // Call to a direct offset
      vm_jmp(vm, arg1);
    } else if (arg0 == 0x01) {
      // Call to the stop of the stack
      if (stack_pop(&vm->main, &aux) == ERROR) {
        printf(
            "error: cannot get return address from stack top, opc: %d, addr: "
            "%p\n",
            opcode, curpos);
        return ERROR;
      }
      vm_jmp(vm, aux);
    }
    break;
  case RET:
    if (stack_pop(&vm->call, &aux) == ERROR) {
      printf("error: cannot get return address, opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }
    vm_jmp(vm, aux);
    break;
  default:
    printf("error: unrecognized or unsupported, opc: %#08x, addr: %p\n", opcode,
           curpos);
    return ERROR;
  }

  if (opcode == PUSH || (opcode >= ADD && opcode <= NOT)) {
    if (stack_push(&vm->main, aux) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
  }

  if (opcode >= BULK && opcode <= INSM) {
    if (stack_push(&vm->mem, aux) == ERROR) {
      printf("error: memory stack overflow, opc: %d, addr: %p\n", opcode,
             curpos);
      return ERROR;
    }
  }

  return SUCCESS;
}

void stack_init(struct stack *s, size_t cap) {
  assert(s != NULL);
  s->bot = malloc(sizeof(int64_t) * cap);
  memset(s->bot, 0L, sizeof(int64_t) * cap);
  s->cap = cap;
  s->top = -1;
}

void stack_free(struct stack *s) {
  if (s->bot != NULL) {
    free(s->bot);
  }
}

void stack_print(struct stack *s) {
  assert(s != NULL);
  printf("\tcap: %ld, used: %ld, bot: %p\n", s->cap, s->top + 1, s->bot);
  if (s->top < 0L) {
    printf("\t\t empty stack\n");
    return;
  }

  for (int i = 0; i < s->top + 1; i++) {
    printf("\t\t %d: %ld, %lu\n", i, s->bot[i], s->bot[i]);
  }
}

retcode stack_push(struct stack *s, int64_t v) {
  assert(s != NULL);
  if (s->top >= s->cap) {
    s->bot = realloc(s->bot, s->cap * 2);
    if (s->bot == NULL) {
      return ERROR;
    }
  }

  s->top++;
  s->bot[s->top] = v;
  return SUCCESS;
}

retcode stack_pop(struct stack *s, int64_t *v) {
  assert(s != NULL);
  if (s->top < 0) {
    return ERROR;
  }

  if (v != NULL) {
    *v = s->bot[s->top];
  }

  s->top--;
  return SUCCESS;
}

retcode vm_init(struct vm *vm, const char *filename) {
  assert(vm != NULL);
  assert(filename != NULL);

  stack_init(&vm->main, 32);
  stack_init(&vm->mem, 1);
  stack_init(&vm->call, 32);
  vm->code = NULL;
  vm->code_size = 0L;
  vm->code_offset = 0L;
  vm->halted = 0;

  FILE *file = fopen(filename, "rb");
  if (file == NULL) {
    perror("open file");
    return ERROR;
  }

  fseek(file, 0L, SEEK_END);
  size_t filelen = ftell(file);
  fseek(file, 0L, SEEK_SET);

  uint8_t *buffer = malloc(sizeof(uint8_t) * filelen);
  if (fread(buffer, sizeof(uint8_t), filelen, file) != filelen) {
    printf("error: could not read complete file!\n");
    return ERROR;
  }

  vm->code = buffer;
  vm->code_size = filelen;
  fclose(file);
  return SUCCESS;
}

void vm_free(struct vm *vm) {
  if (vm->code != NULL) {
    free(vm->code);
  }

  stack_free(&vm->main);
  stack_free(&vm->mem);
  stack_free(&vm->call);
}

inline retcode vm_jmp(struct vm *vm, size_t new_offset) {
  if (new_offset <= (vm->code_size - 4)) {
    vm->code_offset = new_offset;
    return SUCCESS;
  }

  printf("error: cannot jump outside from code segment, code addr: %p, offset: "
         "%ld, new addr: %p\n",
         vm->code, new_offset, vm->code + new_offset);
  return ERROR;
}

retcode vm_run(struct vm *vm) {
  while (vm->halted == 0) {
    if (vm_run_step(vm) == ERROR) {
      return ERROR;
    }
  }

  return SUCCESS;
}

int main(int argc, char **argv) {
  if (argc != 2) {
    printf("usage: %s <chaneque file>\n", argv[0]);
    return 1;
  }
  char *filename = argv[1];
  struct vm vm;
  if (vm_init(&vm, filename) == ERROR) {
    printf("could not initialize vm\n");
    vm_free(&vm);
    return 1;
  }

  if (vm_run(&vm) == ERROR) {
    printf("vm run failed\n");
    vm_free(&vm);
    return 1;
  }

  vm_free(&vm);
  return 0;
}
