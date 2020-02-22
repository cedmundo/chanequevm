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

  union value aux = {0LL};
  union value left = {0LL};
  union value right = {0LL};

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

  // fill left, right arguments when needed from stack
  if ((opcode >= ADD && opcode <= GE) || opcode == BULK) {
    if (stack_pop(&vm->data, &right) == ERROR) {
      printf("error: missing right parameter, opc: %d, addr: %p\n", opcode,
             curpos);
      return ERROR;
    }

    if (stack_pop(&vm->data, &left) == ERROR) {
      printf("error: missing left parameter, opc: %d, addr: %p\n", opcode,
             curpos);
      return ERROR;
    }
  } else if (opcode >= NOT && opcode <= JZ) {
    if (stack_pop(&vm->data, &left) == ERROR) {
      printf("error: missing parameter, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
  }

  // check if there is allocated memory before bulking or inspecting
  if (opcode >= BULK && opcode <= INSM) {
    if (vm->resv_data == NULL || vm->resv_size == 0L) {
      printf("error: no reserved memory, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
  }

  // fill aux param from arg0, next 32-bits or next 64-bits depending on mode
  if (opcode == PUSH || opcode == RESV || opcode == BULK || opcode == CALL ||
      (opcode >= JNZ && opcode <= JMP) || (opcode >= LOAD && opcode <= INSM)) {
    if (mode == 0x00) {
      aux.u64 = arg1;
    } else if (mode == 0x01) {
      curpos = vm->code + vm->code_offset;
      aux.u64 = decode_u32(curpos);
      vm->code_offset += 4;
    } else if (mode == 0x02) {
      curpos = vm->code + vm->code_offset;
      aux.u64 = decode_u64(curpos);
      vm->code_offset += 8;
    } else {
      printf("error: unknown mode %d, opc: %d, addr: %p\n", mode, opcode,
             curpos);
      return ERROR;
    }
  }

#ifdef CVM_PSTEP
  printf("read: %02hhX %02hhX %02hhX %02hhX\n", curpos[0], curpos[1], curpos[2],
         curpos[3]);
  printf("run: opcode=%02hhX, mode=%02hhX, arg1=%" PRIu64 "\n", opcode, mode,
         aux.u64);
#endif

  switch ((enum opcode)opcode) {
  case NOP:
    break;
  case HALT:
    vm->halted = 1;
    if (vm->resv_data != NULL) {
      free(vm->resv_data);
      vm->resv_data = NULL;
      vm->resv_size = 0L;
    }

    printf("vm has been halted\n");
    break;
  case CLEAR_STACK:
    while (stack_pop(&vm->data, &aux) != ERROR)
      ;
    break;
  case PRINT_STATE:
    printf("============= vm state =============\n");
    printf("code section: %p, size: %ld, offset: %ld\n", vm->code,
           vm->code_size, vm->code_offset);
    if (vm->resv_data != NULL) {
      printf("total memory requested: %ld bytes on %p\n", vm->resv_size,
             vm->resv_data);
    }

    printf("data stack:\n");
    stack_print(&vm->data);

    printf("call stack:\n");
    stack_print(&vm->call);
    printf("\n====================================\n");
    break;
  case PUSH:
    break;
  case POP:
    stack_pop(&vm->data, &aux);
    break;
  case SWAP:
    stack_swap(&vm->data);
    break;
  case ROT3:
    stack_rot3(&vm->data);
    break;
  case ADD:
    value_op(+, mode, aux, left, right);
    break;
  case SUB:
    value_op(-, mode, aux, left, right);
    break;
  case MUL:
    value_op(*, mode, aux, left, right);
    break;
  case DIV:
    if (right.u64 == 0LL) {
      printf("error: divide by zero, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    value_op(/, mode, aux, left, right);
    break;
  case MOD:
    if (right.u64 == 0LL) {
      printf("error: modulo by zero, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    value_op_nof(%, mode, aux, left, right);
    break;
  case AND:
    value_op_nof(&, mode, aux, left, right);
    break;
  case OR:
    value_op_nof(|, mode, aux, left, right);
    break;
  case XOR:
    value_op_nof (^, mode, aux, left, right);
    break;
  case NEQ:
    value_op(!=, mode, aux, left, right);
    break;
  case EQ:
    value_op(==, mode, aux, left, right);
    break;
  case LT:
    value_op(<, mode, aux, left, right);
    break;
  case LE:
    value_op(<=, mode, aux, left, right);
    break;
  case GT:
    value_op(>, mode, aux, left, right);
    break;
  case GE:
    value_op(>=, mode, aux, left, right);
    break;
  case NOT:
    switch (mode) {
    case 0x00:
      aux.u8 = ~left.u8;
      break;
    case 0x01:
      aux.u16 = ~left.u16;
      break;
    case 0x02:
      aux.u32 = ~left.u32;
      break;
    case 0x03:
      aux.u64 = ~left.u64;
      break;
    case 0x04:
      aux.i8 = ~left.i8;
      break;
    case 0x05:
      aux.i16 = ~left.i16;
      break;
    case 0x06:
      aux.i32 = ~left.i32;
      break;
    case 0x07:
      aux.i64 = ~left.i64;
      break;
    default:
      assert(0 && "unreachable code");
      break;
    }
    break;
  case JNZ:
    if (left.u64 != 0LL) {
      vm_jmp(vm, aux.u64);
    }
    stack_push(&vm->data, left);
    break;
  case JZ:
    if (left.u64 == 0LL) {
      vm_jmp(vm, aux.size);
    }
    stack_push(&vm->data, left);
    break;
  case JMP:
    vm_jmp(vm, aux.size);
    break;
  case CALL:
    if (stack_push(&vm->call, (union value)vm->code_offset) == ERROR) {
      printf("error: cannot call %lu because call stack is overflown, opc: %d, "
             "addr: %p\n",
             aux.size, opcode, curpos);
      return ERROR;
    }
    vm_jmp(vm, aux.size);
    break;
  case RET:
    if (stack_pop(&vm->call, &aux) == ERROR) {
      printf("error: cannot ret because call stack is empty, opc: %d, "
             "addr: %p\n",
             opcode, curpos);
      return ERROR;
    }
    vm_jmp(vm, aux.size);
    break;
  case RESV:
    if (vm->resv_data != NULL) {
      vm->resv_data =
          realloc(vm->resv_data, sizeof(uint8_t) * vm->resv_size + aux.size);
      vm->resv_size = vm->resv_size + aux.size;
    } else {
      vm->resv_data = malloc(sizeof(uint8_t) * aux.size);
      vm->resv_size = aux.size;
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
    if (left.size >= vm->code_size) {
      printf("error: trying to copy byets from outside of code segment, opc: "
             "%d, addr: %p\n",
             opcode, curpos);
      return ERROR;
    }

    if (right.size > vm->resv_size) {
      printf("error: trying to copy a block bigger than allocated, opc: %d, "
             "addr: %p\n",
             opcode, curpos);
      return ERROR;
    }

    memcpy(vm->resv_data + aux.size, vm->code + left.size, right.size);
    break;
  case LOAD:
    right.size = *(vm->resv_data + aux.size);
    if (stack_push(&vm->data, right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
    break;
  case STORE:
    if (stack_pop(&vm->data, &right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    *(vm->resv_data + aux.size) = right.u64;
    break;
  case INSM:
    printf("============= memory inspect =============\n");
    printf("%ld bytes of memory on "
           "%p: \n",
           vm->resv_size, vm->resv_data);
    for (size_t i = 0; i < vm->resv_size; i++) {
      printf("%02hhX ", vm->resv_data[i]);
    }
    printf("\n====================================\n");
    break;
  default:
    printf("error: unrecognized or unsupported opc: %#02x, addr: %p\n", opcode,
           curpos);
    return ERROR;
  }

  if (opcode == PUSH || (opcode >= ADD && opcode <= NOT)) {
    if (stack_push(&vm->data, aux) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
  }

  return SUCCESS;
}

void stack_init(struct stack *s, size_t cap) {
  assert(s != NULL);
  s->bot = malloc(sizeof(union value) * cap);
  memset(s->bot, 0L, sizeof(union value) * cap);
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
    union value cur_val = s->bot[i];
    uint64_t cur = cur_val.u64;
    unsigned char bytes[8];
    printf("\t\t %d: %" PRIu64, i, cur);

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

retcode stack_push(struct stack *s, union value v) {
  assert(s != NULL);
  if (s->top >= s->cap) {
    return ERROR;
  }

  s->top++;
  s->bot[s->top] = v;
  return SUCCESS;
}

retcode stack_pop(struct stack *s, union value *v) {
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

void stack_swap(struct stack *s) {
  if (s->top < 1) {
    return;
  }

  union value a = s->bot[s->top];
  union value b = s->bot[s->top - 1];
  s->bot[s->top] = b;
  s->bot[s->top - 1] = a;
}

void stack_rot3(struct stack *s) {
  if (s->top < 2) {
    return;
  }

  union value a = s->bot[s->top];
  union value b = s->bot[s->top - 1];
  union value c = s->bot[s->top - 2];
  s->bot[s->top] = b;
  s->bot[s->top - 1] = c;
  s->bot[s->top - 2] = a;
}

retcode vm_init(struct vm *vm, const char *filename) {
  assert(vm != NULL);
  assert(filename != NULL);

  stack_init(&vm->data, 32);
  stack_init(&vm->call, 32);
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

  stack_free(&vm->data);
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
