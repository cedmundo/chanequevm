#include "cvm.h"
#include <assert.h>
#include <inttypes.h>
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

  uint8_t *curpos = vm->code + vm->code_offset;
  if (curpos > (vm->code + vm->code_size - 4)) {
    printf("error: no more instructions to read\n");
    return ERROR;
  }
  vm->code_offset += 4;

  uint32_t step = decode_step(curpos);
  uint8_t opcode = decode_opcode(step);
  uint8_t mode = decode_arg0(step);
  uint16_t arg1 = decode_arg1(step);

#ifdef CVM_PSTEP
  printf("feed: %02hhX %02hhX %02hhX %02hhX\n", curpos[0], curpos[1], curpos[2],
         curpos[3]);
  printf("step: opcode=%02hhX, mode=%02hhX, arg1=%d\n", opcode, mode, arg1);
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
    if (vm->resv_data == NULL || vm->resv_size == 0L) {
      printf("error: no allocated memory, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
  }

  if (opcode >= BULK && opcode <= INSM) {
    if (vm->resv_data == NULL) {
      printf("error: cannot bulk without reserving first, opc: %d, addr: %p",
             opcode, curpos);
      return ERROR;
    }
  }

  if (opcode == PUSH || opcode == RESV || (opcode >= JNZ && opcode <= JMP) ||
      (opcode >= LOAD && opcode <= INSM)) {
    if (mode == 0x00) {
      aux = arg1;
    } else if (mode == 0x01) {
      curpos = vm->code + vm->code_offset;
      aux = decode_u32(curpos);
      vm->code_offset += 4;
    } else if (mode == 0x02) {
      curpos = vm->code + vm->code_offset;
      aux = decode_u64(curpos);
      vm->code_offset += 8;
    } else {
      printf("error: unknown mode %d, opc: %d, addr: %p\n", mode, opcode,
             curpos);
      return ERROR;
    }
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
    if (vm->resv_data != NULL) {
      printf("total memory requested: %ld bytes on %p\n", vm->resv_size,
             vm->resv_data);
    }
    printf("main stack:\n");
    stack_print(&vm->main);
    break;
  case PUSH:
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
      vm_jmp(vm, aux);
    }
    stack_push(&vm->main, left);
    break;
  case JZ:
    if (left == 0) {
      vm_jmp(vm, aux);
    }
    stack_push(&vm->main, left);
    break;
  case JMP:
    vm_jmp(vm, aux);
    break;
  case RESV:
    if (vm->resv_data != NULL) {
      // TODO: Think about this
      vm->resv_data =
          realloc(vm->resv_data, sizeof(uint8_t) * vm->resv_size + aux);
      vm->resv_size = vm->resv_size + aux;
    } else {
      vm->resv_data = malloc(sizeof(uint8_t) * aux);
      vm->resv_size = aux;
    }
    break;
  case FREE:
    if (vm->resv_data != NULL) {
      free(vm->resv_data);
    }
    vm->resv_data = NULL;
    vm->resv_size = 0L;
    break;
  case BULK:
    if ((size_t)left >= vm->code_size) {
      printf(
          "error: trying to copy outside of code segment, opc: %d, addr: %p\n",
          opcode, curpos);
      return ERROR;
    }

    memcpy(vm->resv_data, vm->code + left, right);
    break;
  case LOAD:
    right = *(vm->resv_data + aux);
    if (stack_push(&vm->main, right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
    break;
  case STORE:
    if (stack_pop(&vm->main, &right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    *(vm->resv_data + aux) = right;
    break;
  case INSM:
    printf("============= memory inspect =============\n");
    printf("%ld bytes of memory on "
           "%p: \n",
           vm->resv_size, vm->resv_data);
    for (size_t i = 0; i < vm->resv_size; i++) {
      printf("%02hhX ", vm->resv_data[i]);
    }
    printf("\n============= \n");
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
  printf("\tcap: %" PRId64 ", used: %" PRId64 ", bot: %p\n", s->cap, s->top + 1,
         s->bot);
  if (s->top < 0L) {
    printf("\t\t empty stack\n");
    return;
  }

  for (int i = 0; i < s->top + 1; i++) {
    int64_t cur = s->bot[i];
    unsigned char bytes[8];
    printf("\t\t %d: %" PRId64 ", %" PRIu64, i, cur, cur);

    bytes[0] = (cur >> 56) & 0xFF;
    bytes[1] = (cur >> 48) & 0xFF;
    bytes[2] = (cur >> 40) & 0xFF;
    bytes[3] = (cur >> 32) & 0xFF;
    bytes[4] = (cur >> 24) & 0xFF;
    bytes[5] = (cur >> 16) & 0xFF;
    bytes[6] = (cur >> 8) & 0xFF;
    bytes[7] = cur & 0xFF;
    printf(" [%02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX %02hhX]\n",
           bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6],
           bytes[7]);
  }
}

retcode stack_push(struct stack *s, int64_t v) {
  assert(s != NULL);
  if (s->top >= s->cap) {
    return ERROR;
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
  vm->code = NULL;
  vm->code_size = 0L;
  vm->code_offset = 0L;
  vm->halted = 0;
  vm->resv_size = 0L;
  vm->resv_data = NULL;

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

  if (vm->resv_data != NULL) {
    free(vm->resv_data);
  }

  stack_free(&vm->main);
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
