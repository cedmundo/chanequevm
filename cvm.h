#ifndef CVM_H
#define CVM_H
#include <stddef.h>
#include <stdint.h>

struct stack {
  int64_t *bot;
  size_t top;
  size_t cap;
};

// this is an stack-based virtual machine
//  note: for the moment we are only going to support integer operands
struct vm {
  struct stack main; // main ops stack
  struct stack mem;  // runtime allocated memory
  int halted;
  uint8_t *code;
  size_t code_size;
  size_t code_offset;
};

enum opcode {
  NOP = 0x00,
  HALT = 0x01,
  CLEAR_STACK = 0x02,
  PRINT_STATE = 0x03,
  PUSH = 0x04,
  POP = 0x05,

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

retcode stack_push(struct stack *s, int64_t v);
retcode stack_pop(struct stack *s, int64_t *v);

retcode vm_init(struct vm *vm, const char *filename);
void vm_free(struct vm *vm);
retcode vm_run_step(struct vm *vm);
retcode vm_run(struct vm *vm);

retcode vm_jmp(struct vm *vm, size_t new_offset);

#define decode_step(bytes)                                                     \
  (bytes[0] + ((uint32_t)bytes[1] << 8) + ((uint32_t)bytes[2] << 16) +         \
   ((uint32_t)bytes[3] << 24))

#define decode_opcode(step) (step >> 24)
#define decode_arg0(step) ((step & 0x00FF0000) >> 16)
#define decode_arg1(step) ((step & 0x0000FFFF))

#endif /* CVM_H */
