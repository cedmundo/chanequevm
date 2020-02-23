#include "cvm.h"
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

inline retcode vm_run_step(struct vm *vm) {
  assert(vm != NULL);

  if (vm->halted) {
    fprintf(stderr, "error: already halted vm\n");
    return ERROR;
  }

  union value aux = {0LL};
  union value left = {0LL};
  union value right = {0LL};

  uint8_t *curpos = vm->code + vm->code_offset;
  if (curpos > (vm->code + vm->code_size - 4)) {
    fprintf(stderr, "error: no more instructions to read\n");
    vm->halted = 1;
    return ERROR;
  }
  vm->code_offset += 4;

  uint32_t step = decode_step(curpos);
  uint8_t opcode = decode_opcode(step);
  uint8_t mode = decode_arg0(step);
  uint16_t arg1 = decode_arg1(step);

  // fill left, right arguments when needed from stack
  if ((opcode >= ADD && opcode <= GE) || opcode == BULK || opcode == SETERR) {
    if (stack_pop(&vm->data, &right) == ERROR) {
      vm_set_error(vm, 0x10,
                   "missing stack right parameter (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }

    if (stack_pop(&vm->data, &left) == ERROR) {
      vm_set_error(vm, 0x10,
                   "missing stack left parameter (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }
  } else if (opcode >= NOT && opcode <= JZ) {
    if (stack_pop(&vm->data, &left) == ERROR) {
      vm_set_error(vm, 0x11,
                   "missing stack parameter (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }
  }

  // check if there is allocated memory before bulking or inspecting
  if (opcode >= BULK && opcode <= INSM) {
    if (vm->resv_data == NULL || vm->resv_size == 0L) {
      vm_set_error(vm, 0x12,
                   "no previously reserved memory (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }
  }

  // fill aux param from arg0, next 32-bits or next 64-bits depending on mode
  if (opcode == PUSH || opcode == RESV || opcode == BULK || opcode == CALL ||
      (opcode >= JNZ && opcode <= JMP) || opcode == SETHDLR ||
      (opcode >= LOAD && opcode <= INSM)) {
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
      vm_set_error(vm, 0x13,
                   "unknown mode for feed (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
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

    if (vm->error_handler != 0) {
      printf("error handler at: %p (%p + %lu)\n", vm->code + vm->error_handler,
             vm->code, vm->error_handler);
    }

    if (vm->error_code != 0) {
      printf("\t[on error] code: 0x%08x, message: %s\n", vm->error_code,
             vm->error_message);
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
      vm_set_error(vm, 0x15,
                   "divide by zero (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }

    value_op(/, mode, aux, left, right);
    break;
  case MOD:
    if (right.u64 == 0LL) {
      vm_set_error(vm, 0x15,
                   "modulo by zero (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
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
      vm_set_error(vm, 0x16,
                   "cannot call %lu because stack is overflown (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   aux.size, opcode, mode, arg1);
      return ERROR;
    }
    vm_jmp(vm, aux.size);
    break;
  case RET:
    if (stack_pop(&vm->call, &aux) == ERROR) {
      vm_set_error(vm, 0x15,
                   "cannot ret because stack is empty (opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
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
      vm_set_error(vm, 0x18,
                   "cannot copy bytes that are outside code range "
                   "(opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }

    if (right.size > vm->resv_size - aux.size) {
      vm_set_error(vm, 0x19,
                   "cannot copy bytes bigger than available space "
                   "(opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }

    memcpy(vm->resv_data + aux.size, vm->code + left.size, right.size);
    break;
  case LOAD:
    right.size = *(vm->resv_data + aux.size);
    if (stack_push(&vm->data, right) == ERROR) {
      vm_set_error(vm, 0x20,
                   "stack overflow on load "
                   "(opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
      return ERROR;
    }
    break;
  case STORE:
    if (stack_pop(&vm->data, &right) == ERROR) {
      vm_set_error(vm, 0x21,
                   "empty stack for store "
                   "(opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
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
  case SETHDLR:
    vm->error_handler = aux.size;
    break;
  case SETERR:
    vm_set_error(vm, left.i32, "%s", (char *)vm->resv_data + right.size);
    return ERROR;
  case CLRERR:
    if (vm->error_message != NULL && vm->should_free_error) {
      free(vm->error_message);
    }

    vm->should_free_error = 0;
    vm->error_message = NULL;
    vm->error_code = 0;
    break;
  default:
    printf("unrecognized or unsupported opc: %#02x, addr: %p\n", opcode,
           curpos);
    return ERROR;
  }

  if (opcode == PUSH || (opcode >= ADD && opcode <= NOT)) {
    if (stack_push(&vm->data, aux) == ERROR) {
      vm_set_error(vm, 0x20,
                   "stack overflow "
                   "(opcode=%02hhX, "
                   "mode=%02hhX, arg1=%" PRIu64 ")",
                   opcode, mode, arg1);
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

  stack_init(&vm->data, 4);
  stack_init(&vm->call, 4);
  vm->code = NULL;
  vm->code_size = 0L;
  vm->code_offset = 0L;
  vm->halted = 0;
  vm->resv_size = 0L;
  vm->resv_data = NULL;
  vm->error_handler = 0L;
  vm->error_message = NULL;
  vm->error_code = 0;
  vm->should_free_error = 0;

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
    fprintf(stderr, "error: could not read complete file!\n");
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

  if (vm->error_message != NULL && vm->should_free_error) {
    free(vm->error_message);
  }

  stack_free(&vm->data);
  stack_free(&vm->call);
}

void vm_set_error(struct vm *vm, int error_code, const char *user_format, ...) {
  if (vm->error_message != NULL) {
    fprintf(stderr,
            "error: only one error can be handled right now (at %p + %lu)\n",
            vm->code, vm->code_offset);
    return;
  }

  size_t user_format_len = strlen(user_format);
  char *format = malloc(user_format_len + 80);
  char *error_message = malloc(MAX_ERROR_MESSAGE_LEN);
  char *position = malloc(50);
  sprintf(position, " (at %p + %lu)(code %d)\n", vm->code, vm->code_offset,
          error_code);
  memset(format, 0L, user_format_len + 50);
  memcpy(format, user_format, strlen(user_format));
  memcpy(format + user_format_len, position, 50);
  free(position);

  va_list args;
  va_start(args, user_format);
  vsprintf(error_message, format, args);
  va_end(args);
  free(format);

  vm->should_free_error = 1;
  vm->error_message = error_message;
  vm->error_code = error_code;
}

inline retcode vm_jmp(struct vm *vm, size_t new_offset) {
  if (new_offset <= (vm->code_size - 4)) {
    vm->code_offset = new_offset;
    return SUCCESS;
  }

  vm_set_error(vm, 0x22, "cannot jump outside code segment");
  return ERROR;
}

retcode vm_run(struct vm *vm) {
  while (vm->halted == 0) {
    retcode rc = vm_run_step(vm);
    if (rc == ERROR && vm->error_handler != 0L) {
#ifdef CVM_PRINT_ALL_ERRORS
      fprintf(stderr, "error: %s\n", vm->error_message);
#endif
      if (stack_push(&vm->data, (union value)vm->error_code) == ERROR) {
        fprintf(stderr, "error: cannot push error code for handler\n");
        vm->halted = 1;
        return ERROR;
      }

      if (stack_push(&vm->call, (union value)vm->code_offset) == ERROR) {
        fprintf(stderr, "error: cannot push return address for handler\n");
        vm->halted = 1;
        return ERROR;
      }

      vm_jmp(vm, vm->error_handler);
    } else if (rc == ERROR) {
      fprintf(stderr, "error: handler not present, halting machine\n");
      vm->halted = 1;
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
    fprintf(stderr, "could not initialize vm\n");
    vm_free(&vm);
    return 1;
  }

  if (vm_run(&vm) == ERROR) {
    fprintf(stderr, "vm run failed\n");
    vm_free(&vm);
    return 1;
  }

  vm_free(&vm);
  return 0;
}
