#ifndef CVM_H
#define CVM_H
#include <stddef.h>
#include <stdint.h>

union value {
  uint8_t u8;
  uint16_t u16;
  uint32_t u32;
  uint64_t u64;
  int8_t i8;
  int16_t i16;
  int32_t i32;
  int64_t i64;
  float f32;
  double f64;
  size_t size;
};

struct stack {
  union value *bot;
  int64_t top;
  int64_t cap;
};

struct vm {
  struct stack data; // data stack, main operation source
  struct stack call; // call stack, where return addresses are stored
  int halted;
  uint8_t *code;
  size_t code_size;
  size_t code_offset;
  size_t resv_size;
  uint8_t *resv_data;
};

enum opcode {
  NOP = 0x00,
  HALT = 0x01,
  CLEAR_STACK = 0x02,
  PRINT_STATE = 0x03,
  PUSH = 0x04,
  POP = 0x05,
  SWAP = 0x06,
  ROT3 = 0x07,

  /* With two stack arguments */
  ADD = 0x10,
  SUB = 0x11,
  DIV = 0x12,
  MUL = 0x13,
  MOD = 0x14,

  AND = 0x15,
  OR = 0x1A,
  XOR = 0x1B,

  NEQ = 0x1C,
  EQ = 0x1D,
  LT = 0x1F,
  LE = 0x20,
  GT = 0x21,
  GE = 0x22,

  /* With one stack argument */
  NOT = 0x30,
  JNZ = 0x31,
  JZ = 0x32,

  /* Without any stack arguments */
  JMP = 0x33,
  CALL = 0x35,
  RET = 0x36,

  /* Memory stack */
  RESV = 0x40,
  FREE = 0x41,
  BULK = 0x42,
  LOAD = 0x43,
  STORE = 0x44,
  INSM = 0x45,
};

typedef enum retcode { ERROR, SUCCESS } retcode;

void stack_init(struct stack *s, size_t cap);
void stack_free(struct stack *s);
void stack_print(struct stack *s);

retcode stack_push(struct stack *s, union value v);
retcode stack_pop(struct stack *s, union value *v);
void stack_swap(struct stack *s);
void stack_rot3(struct stack *s);

retcode vm_init(struct vm *vm, const char *filename);
void vm_free(struct vm *vm);
retcode vm_run_step(struct vm *vm);
retcode vm_run(struct vm *vm);

retcode vm_jmp(struct vm *vm, size_t new_offset);

#define decode_u32(bytes)                                                      \
  (bytes[0] + ((uint32_t)bytes[1] << 8) + ((uint32_t)bytes[2] << 16) +         \
   ((uint32_t)bytes[3] << 24))

#define decode_u64(bytes)                                                      \
  (bytes[0] + ((uint64_t)bytes[1] << 8) + ((uint64_t)bytes[2] << 16) +         \
   ((uint64_t)bytes[3] << 24)) +                                               \
      ((uint64_t)bytes[4] << 32) + ((uint64_t)bytes[5] << 40) +                \
      ((uint64_t)bytes[6] << 48) + ((uint64_t)bytes[7] << 56)

#define decode_step(bytes) decode_u32(bytes)
#define decode_opcode(step) (step >> 24)
#define decode_arg0(step) ((step & 0x00FF0000) >> 16)
#define decode_arg1(step) ((step & 0x0000FFFF))

#define value_op(op, mode, aux, left, right)                                   \
  do {                                                                         \
    switch (mode) {                                                            \
    case 0x00:                                                                 \
      aux.u8 = left.u8 op right.u8;                                            \
      break;                                                                   \
    case 0x01:                                                                 \
      aux.u16 = left.u16 op right.u16;                                         \
      break;                                                                   \
    case 0x02:                                                                 \
      aux.u32 = left.u32 op right.u32;                                         \
      break;                                                                   \
    case 0x03:                                                                 \
      aux.u64 = left.u64 op right.u64;                                         \
      break;                                                                   \
    case 0x04:                                                                 \
      aux.i8 = left.i8 op right.i8;                                            \
      break;                                                                   \
    case 0x05:                                                                 \
      aux.i16 = left.i16 op right.i16;                                         \
      break;                                                                   \
    case 0x06:                                                                 \
      aux.i32 = left.i32 op right.i32;                                         \
      break;                                                                   \
    case 0x07:                                                                 \
      aux.i64 = left.i64 op right.i64;                                         \
      break;                                                                   \
    case 0x08:                                                                 \
      aux.f32 = left.f32 op right.f32;                                         \
      break;                                                                   \
    case 0x09:                                                                 \
      aux.f64 = left.f64 op right.f64;                                         \
      break;                                                                   \
    default:                                                                   \
      assert(0 && "unreachable code");                                         \
      break;                                                                   \
    }                                                                          \
  } while (0);

#define value_op_nof(op, mode, aux, left, right)                               \
  do {                                                                         \
    switch (mode) {                                                            \
    case 0x00:                                                                 \
      aux.u8 = left.u8 op right.u8;                                            \
      break;                                                                   \
    case 0x01:                                                                 \
      aux.u16 = left.u16 op right.u16;                                         \
      break;                                                                   \
    case 0x02:                                                                 \
      aux.u32 = left.u32 op right.u32;                                         \
      break;                                                                   \
    case 0x03:                                                                 \
      aux.u64 = left.u64 op right.u64;                                         \
      break;                                                                   \
    case 0x04:                                                                 \
      aux.i8 = left.i8 op right.i8;                                            \
      break;                                                                   \
    case 0x05:                                                                 \
      aux.i16 = left.i16 op right.i16;                                         \
      break;                                                                   \
    case 0x06:                                                                 \
      aux.i32 = left.i32 op right.i32;                                         \
      break;                                                                   \
    case 0x07:                                                                 \
      aux.i64 = left.i64 op right.i64;                                         \
      break;                                                                   \
    default:                                                                   \
      assert(0 && "unreachable code");                                         \
      break;                                                                   \
    }                                                                          \
  } while (0);

#endif /* CVM_H */
