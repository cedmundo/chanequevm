#ifndef CVM_H
#define CVM_H
#include <stddef.h>
#include <stdint.h>

struct stack {
  int32_t *bot;
  size_t top;
  size_t cap;
};

// this is an stack-based virtual machine
//  note: for the moment we are only going to support integer operands
struct vm {
  struct stack main;
  int halted;
  uint8_t *code;
  size_t code_size;
  size_t code_offset;
};

enum opcode {
  NOP = 0x0,
  HALT = 0x1,
  CLEAR_STACK = 0x2,
  PRINT_STATE = 0x3,
  PUSH = 0x4,
  POP = 0xAA,

  /* With two stack arguments */
  ADD = 0x5,
  SUB = 0x6,
  DIV = 0x8,
  MUL = 0x9,
  MOD = 0xA,

  AND = 0xB,
  OR = 0xC,
  XOR = 0xE,

  NEQ = 0xF,
  EQ = 0x10,
  LT = 0x11,
  LE = 0x12,
  GT = 0x13,
  GE = 0x14,

  /* With one stack argument */
  NOT = 0x15,
  JNZ = 0x16,
  JZ = 0x17,

  /* Without any stack arguments */
  JMP = 0xAB,
};

typedef enum retcode { ERROR, SUCCESS } retcode;

int32_t dec_i32(uint8_t bytes[]);
uint64_t dec_u64(uint8_t bytes[]);

void stack_init(struct stack *s, size_t cap);
void stack_free(struct stack *s);
void stack_print(struct stack *s);

retcode stack_push(struct stack *s, int32_t v);
retcode stack_pop(struct stack *s, int32_t *v);

retcode vm_init(struct vm *vm, const char *filename);
void vm_free(struct vm *vm);
retcode vm_run_step(struct vm *vm);
retcode vm_run(struct vm *vm);

retcode vm_jmp(struct vm *vm, size_t new_offset);

#define get_arg0(i, o) (int32_t)(i & ~((uint64_t)o << 56))
#endif /* CVM_H */
