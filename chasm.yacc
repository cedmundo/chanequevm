%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <stdint.h>

char *strval;
int yylex();
int yyerror(const char* s);

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
  F64
};

static char* s_modes[] = {
  "U8", "U16", "U32", "U64",
  "I8", "I16", "I32", "I64",
  "F32", "F64", NULL
};

static int i_modes[] = {
  0x00, 0x01, 0x02, 0x03,
  0x04, 0x05, 0x06, 0x07,
  0x08, 0x09
};

enum mnemonic {
  NOP, HALT, CLEAR_STACK, PRINT_STATE, PUSH, POP,
  SWAP, ROT3, ADD, SUB, DIV, MUL, MOD, AND, OR,
  XOR, NEQ, EQ, LT, LE, GT, GE, NOT, JNZ, JZ, JMP,
  CALL, RET, RESV, FREE, BULK, LOAD, STORE, INSM,
  SETHDLR, SETERR, CLRERR, DATA
};

static char* s_mnemonics[] = {
  "NOP", "HALT", "CLRSTACK", "PS", "PUSH", "POP",
  "SWAP", "ROT3", "ADD", "SUB", "DIV", "MUL", "MOD", "AND", "OR",
  "XOR", "NEQ", "EQ", "LT", "LE", "GT", "GE", "NOT", "JNZ", "JZ", "JMP",
  "CALL", "RET", "RESV", "FREE", "BULK", "LOAD", "STORE", "INSM",
  "SETHDLR", "SETERR", "CLRERR", "DATA", NULL
};

static int i_opcodes[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B,
  0x1C, 0x1D, 0x1F, 0x20, 0x21, 0x22, 0x30, 0x31, 0x32, 0x33, 0x35,
  0x36, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
  0x50, 0x51, 0x52, 0x00
};

struct instruction {
  const char* label;
  enum mnemonic mnemonic;
  enum mode mode;
  union value arg1;
  struct instruction *next;
};

struct source {
  struct instruction *instructions;
};

char *strdup(const char *src) {
  char *dst = malloc(strlen (src) + 1);
  if (dst == NULL) {
    return NULL;
  }

  memset(dst, 0L, strlen(src) + 1);
  strcpy(dst, src);
  return dst;
}

%}

%start source
%token ID COLON AMP NUMBER STRING ENDL

%%

source: instructions
instructions: %empty
  | instruction instructions
  | label instruction instructions
  ;

label:
  ID COLON
  | ID COLON ENDL
  ;

mode: ID
arg1: AMP ID | ID | NUMBER | STRING

instruction:
  ID mode arg1 ENDL
  | ID arg1 ENDL
  | ID ENDL
  ;

%%

int main() {
  int res = yyparse();
  if (res != 0) {
    return res;
  }

  return 0;
}

int yyerror(const char *s) {
  fprintf(stderr, "%s\n",s);
  return 0;
}

int yywrap() {
  return 1;
}
