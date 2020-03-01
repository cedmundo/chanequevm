#ifndef CHASM_H
#define CHASM_H
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
  char *str;
};

enum mode {
  U8,
  U16,
  U32,
  U64,
  I8,
  I16,
  I32,
  I64,
  F32,
  F64,
  STR,
  WORD,
  DWORD,
  QWORD
};

struct typed_value {
  union value value;
  enum mode mode;
  int is_ref;
};

enum mnemonic {
  NOP,
  HALT,
  CLRS,
  PSTATE,
  PUSH,
  POP,
  SWAP,
  ROT3,
  ADD,
  SUB,
  DIV,
  MUL,
  MOD,
  AND,
  OR,
  XOR,
  NEQ,
  EQ,
  LT,
  LE,
  GT,
  GE,
  NOT,
  JNZ,
  JZ,
  JMP,
  CALL,
  RET,
  LOAD,
  STORE,
  PSEG,
  SETHDLR,
  SETERR,
  CLRERR,
  DATA
};

struct instruction {
  const char *label;
  enum mnemonic mnemonic;
  enum mode mode;
  struct typed_value arg1;
  struct instruction *next;
  size_t feed_size;
  size_t offset;
  size_t size;
};

struct label_location {
  const char *label;
  size_t offset;
  struct label_location *next;
};

struct source {
  struct instruction *instructions;
  struct label_location *label_locations;
  size_t output_size;
};

#endif
