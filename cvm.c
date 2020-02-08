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

  struct variant aux = {.as_u64 = 0L, .type = VAR_NONE};
  struct variant left = {.as_u64 = 0L, .type = VAR_NONE};
  struct variant right = {.as_u64 = 0L, .type = VAR_NONE};
  struct variant zero = {.as_u64 = 0L, .type = VAR_NONE};
  struct memchunk chunk = {.addr = NULL, .size = 0L};
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

    udata = aux.as_addr;
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
    aux.as_u16 = arg1;
    aux.type = VAR_U16;
    break;
  case POP:
    stack_pop(&vm->main, &aux);
    break;
  case ADD:
    aux = variant_add(left, right);
    break;
  case SUB:
    aux = variant_sub(left, right);
    break;
  case MUL:
    aux = variant_mul(left, right);
    break;
  case DIV:
    aux = variant_div(left, right);
    break;
  case MOD:
    aux = variant_mod(left, right);
    break;
  case AND:
    aux = variant_and(left, right);
    break;
  case OR:
    aux = variant_or(left, right);
    break;
  case XOR:
    aux = variant_xor(left, right);
    break;
  case NEQ:
    aux = variant_neq(left, right);
    break;
  case EQ:
    aux = variant_eq(left, right);
    break;
  case LT:
    aux = variant_lt(left, right);
    break;
  case LE:
    aux = variant_le(left, right);
    break;
  case GT:
    aux = variant_gt(left, right);
    break;
  case GE:
    aux = variant_ge(left, right);
    break;
  case NOT:
    aux = variant_not(left);
    break;
  case JNZ:
    if (variant_neq(left, zero).as_u64) {
      vm_jmp(vm, arg1);
    }
    stack_push(&vm->main, left);
    break;
  case JZ:
    if (variant_eq(left, zero).as_u64) {
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
    chunk.addr = udata;
    chunk.size = sizeof(uint8_t) * arg1;
    aux.as_chunk = chunk;
    aux.type = VAR_CHUNK;
    if (stack_push(&vm->mem, aux) == ERROR) {
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
    if (aux.type != VAR_CHUNK) {
      printf("error: trying to free a type other than a chunk, opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }

    free(aux.as_chunk.addr);
    break;
  case BULK:
    if (left.as_offset >= vm->code_size) {
      printf(
          "error: trying to copy outside of code segment, opc: %d, addr: %p\n",
          opcode, curpos);
      return ERROR;
    }

    if (udata == NULL) {
      printf(
          "error: trying to do a copy into a null pointer, opc: %d, addr: %p",
          opcode, curpos);
      return ERROR;
    }

    if (right.as_offset > aux.as_chunk.size) {
      printf("error: trying to do a copy a buffer greater than allocated, opc: "
             "%d, addr: %p",
             opcode, curpos);
      return ERROR;
    }

    memcpy(udata, vm->code + left.as_offset, right.as_offset);
    break;
  case LOAD:
    // LOAD [TYPE] [OFFSET WITHIN MEMORY]
    if (udata == NULL) {
      printf("error: trying to do a copy from a null pointer,  opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }
    assert(0 && "load is not supported yet (variants will break it)");
    assert(arg0 == 0x00 && "only 16-bit offsets are supported");

    right = *((struct variant *)udata + arg1);
    if (stack_push(&vm->main, right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }
    break;
  case STORE:
    // STORE [TYPE] [OFFSET WITHIN MEMORY]
    if (udata == NULL) {
      printf("error: trying to do a copy from a null pointer,  opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }

    assert(0 && "store is not supported yet (variants will break it)");
    if (stack_pop(&vm->main, &right) == ERROR) {
      printf("error: stack overflow, opc: %d, addr: %p\n", opcode, curpos);
      return ERROR;
    }

    *((struct variant *)udata + arg1) = right;
    break;
  case INSM:
    if (udata == NULL) {
      printf("error: trying to do a copy from a null pointer,  opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }

    printf("inspect: %p (%p + %d) = ", (udata + arg1), udata, arg1);
    variant_print(*((struct variant *)udata + arg1));
    break;
  case CALL:
    left.type = VAR_OFFSET;
    left.as_offset = vm->code_offset;
    if (stack_push(&vm->call, left) == ERROR) {
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
      vm_jmp(vm, aux.as_offset);
    }
    break;
  case RET:
    if (stack_pop(&vm->call, &aux) == ERROR) {
      printf("error: cannot get return address, opc: %d, addr: "
             "%p\n",
             opcode, curpos);
      return ERROR;
    }
    vm_jmp(vm, aux.as_offset);
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
  s->bot = malloc(sizeof(struct variant) * cap);
  memset(s->bot, 0L, sizeof(struct variant) * cap);
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
    printf("\t\t %d: ", i + 1);
    variant_print(s->bot[i]);
  }
}

void variant_print(struct variant v) {
  switch (v.type) {
  case VAR_NONE:
    printf("[none]\n");
    break;
  case VAR_ERROR:
    printf("[error] %s\n", v.as_error);
    break;
  case VAR_U8:
    printf("[u8] %u\n", v.as_u8);
    break;
  case VAR_U16:
    printf("[u16] %u\n", v.as_u16);
    break;
  case VAR_U32:
    printf("[u32] %u\n", v.as_u32);
    break;
  case VAR_U64:
    printf("[u64] %lu\n", v.as_u64);
    break;
  case VAR_I8:
    printf("[i8] %d\n", v.as_i8);
    break;
  case VAR_I16:
    printf("[i16] %d\n", v.as_i16);
    break;
  case VAR_I32:
    printf("[i32] %d\n", v.as_i32);
    break;
  case VAR_I64:
    printf("[i64] %ld\n", v.as_i64);
    break;
  case VAR_F32:
    printf("[f32] %0.4f\n", v.as_f32);
    break;
  case VAR_F64:
    printf("[f64] %0.4lf\n", v.as_f64);
    break;
  case VAR_ADDR:
    printf("[addr] 0x%p\n", v.as_addr);
    break;
  case VAR_CHUNK:
    printf("[chunk] 0x%p (%ld bytes)\n", v.as_chunk.addr, v.as_chunk.size);
    break;
  case VAR_OFFSET:
    printf("[offset] %ld\n", v.as_offset);
    break;
  default:
    printf("[unknown type]\n");
    break;
  }
}

struct variant variant_add(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == right.type && left.type == VAR_F32) {
    res.as_f32 = left.as_f32 + right.as_f32;
    res.type = VAR_F32;
  } else if (left.type == right.type && left.type == VAR_F64) {
    res.as_f64 = left.as_f64 + right.as_f64;
    res.type = VAR_F64;
  } else if (left.type == VAR_CHUNK || right.type == VAR_CHUNK) {
    printf("error: incompatible add operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible add operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 + right.as_u64;
  }
  return res;
}

struct variant variant_sub(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == right.type && left.type == VAR_F32) {
    res.as_f32 = left.as_f32 - right.as_f32;
    res.type = VAR_F32;
  } else if (left.type == right.type && left.type == VAR_F64) {
    res.as_f64 = left.as_f64 - right.as_f64;
    res.type = VAR_F64;
  } else if (left.type == VAR_CHUNK || right.type == VAR_CHUNK) {
    printf("error: incompatible sub operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible sub operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 - right.as_u64;
  }
  return res;
}

struct variant variant_mul(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == right.type && left.type == VAR_F32) {
    res.as_f32 = left.as_f32 * right.as_f32;
    res.type = VAR_F32;
  } else if (left.type == right.type && left.type == VAR_F64) {
    res.as_f64 = left.as_f64 * right.as_f64;
    res.type = VAR_F64;
  } else if (left.type == VAR_CHUNK || right.type == VAR_CHUNK) {
    printf("error: incompatible mul operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mul operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 * right.as_u64;
  }
  return res;
}

struct variant variant_div(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == right.type && left.type == VAR_F32) {
    res.as_f32 = left.as_f32 / right.as_f32;
    res.type = VAR_F32;
  } else if (left.type == right.type && left.type == VAR_F64) {
    res.as_f64 = left.as_f64 / right.as_f64;
    res.type = VAR_F64;
  } else if (left.type == VAR_CHUNK || right.type == VAR_CHUNK) {
    printf("error: incompatible div operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible div operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 / right.as_u64;
  }
  return res;
}

struct variant variant_mod(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 % right.as_u64;
  }
  return res;
}

struct variant variant_xor(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 ^ right.as_u64;
  }
  return res;
}

struct variant variant_and(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 & right.as_u64;
  }
  return res;
}

struct variant variant_or(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 | right.as_u64;
  }
  return res;
}

struct variant variant_neq(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 != right.as_u64;
  }
  return res;
}

struct variant variant_eq(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 == right.as_u64;
  }
  return res;
}

struct variant variant_gt(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 > right.as_u64;
  }
  return res;
}

struct variant variant_ge(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 >= right.as_u64;
  }
  return res;
}

struct variant variant_lt(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 < right.as_u64;
  }
  return res;
}

struct variant variant_le(struct variant left, struct variant right) {
  struct variant res;
  if (left.type == VAR_CHUNK || right.type == VAR_CHUNK ||
      right.type == VAR_F32 || right.type == VAR_F64 || left.type == VAR_F32 ||
      left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = left.as_u64 <= right.as_u64;
  }
  return res;
}

struct variant variant_not(struct variant left) {
  struct variant res;
  if (left.type == VAR_CHUNK || left.type == VAR_F32 || left.type == VAR_F64) {
    printf("error: incompatible mod operands\n");
    res.type = VAR_ERROR;
    res.as_error = "incompatible mod operands";
  } else {
    res.type = left.type;
    res.as_u64 = ~left.as_u64;
  }
  return res;
}

retcode stack_push(struct stack *s, struct variant v) {
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

retcode stack_pop(struct stack *s, struct variant *v) {
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
