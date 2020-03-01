%{
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "chasm.h"

char *strval;

const struct typed_value v_zero = { 0L };

static char* s_modes[] = {
  "U8", "U16", "U32", "U64",
  "I8", "I16", "I32", "I64",
  "F32", "F64", "STR",
  "WORD", "DWORD", "QWORD", NULL
};

static int i_modes[] = {
  0x00, 0x01, 0x02, 0x03,
  0x04, 0x05, 0x06, 0x07,
  0x08, 0x09, 0x00,
  0x01, 0x02, 0x03
};

static char* s_mnemonics[] = {
  "NOP", "HALT", "CLRSTACK", "PSTATE", "PUSH", "POP",
  "SWAP", "ROT3", "ADD", "SUB", "DIV", "MUL", "MOD", "AND", "OR",
  "XOR", "NEQ", "EQ", "LT", "LE", "GT", "GE", "NOT", "JNZ", "JZ", "JMP",
  "CALL", "RET", "LOAD", "STORE", "PSEG",
  "SETHDLR", "SETERR", "CLRERR", "DATA", NULL
};

static int i_opcodes[] = {
  0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
  0x06, 0x07, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x1A, 0x1B,
  0x1C, 0x1D, 0x1F, 0x20, 0x21, 0x22, 0x30, 0x31, 0x32, 0x33,
  0x35, 0x36, 0x43, 0x44, 0x45,
  0x50, 0x51, 0x52, 0x00
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

enum mnemonic mnemonic_by_name(const char *wanted) {
  assert(wanted != NULL);
  for(int i=0;s_mnemonics[i] != NULL; i++) {
    if (strcmp(wanted, s_mnemonics[i]) == 0) {
      return i;
    }
  }

  fprintf(stderr, "unknown instruction %s\n", wanted);
  return -1;
}

enum mode mode_by_name(const char *wanted) {
  assert(wanted != NULL);
  for(int i=0;s_modes[i] != NULL; i++) {
    if (strcmp(wanted, s_modes[i]) == 0) {
      return i;
    }
  }

  fprintf(stderr, "unknown mode %s\n", wanted);
  return -1;
}

int yylex();
int yyerror(struct source *, const char*);

%}

%code requires {
  #include "chasm.h"
}

%parse-param {struct source *src}

%start source
%token ID COLON AMP NUMBER STRING ENDL

%union {
  int token;
  char *label;
  char *id;
  enum mode mode;
  enum mnemonic mnemonic;
  struct typed_value arg1;
  struct instruction *instruction;
};

%type<id> id
%type<label> label
%type<mnemonic> iid
%type<mode> mode
%type<token> COLON AMP ENDL NUMBER STRING
%type<arg1> arg1
%type<instruction> instruction instructions
%%

source: instructions { src->instructions = $1; }

instructions:
  ENDL {
    $$ = NULL;
  }
  | instruction ENDL
  {
    $$ = $1;
  }
  | instruction ENDL instructions
  {
    if ($3 != NULL) {
      $1->next = $3;
    }
    $$ = $1;
  }
  | ENDL instructions {
    $$ = $2;
  }
  ;

id: ID { $$ = strdup(strval); }

label:
  id COLON { $$ = $1; }
  | id COLON ENDL { $$ = $1; }
  ;

iid: id
  {
    char *cpy = $1;
    while (*cpy++ = toupper(*cpy));
    $$ = mnemonic_by_name($1);
  }
  ;

mode: id
  {
    char *cpy = $1;
    while (*cpy++ = toupper(*cpy));
    $$ = mode_by_name($1);
  }
  ;

arg1: AMP id
  {
    struct typed_value lit;
    lit.is_ref = 1;
    lit.value.str = $2;
    lit.mode = STR;
    $$ = lit;
  }
  | NUMBER
  {
    struct typed_value lit;
    memset(&lit, 0L, sizeof(struct typed_value));
    lit.is_ref = 0;

    char *valcpy = strdup(strval);
    char *offset = valcpy;
    enum { BIN, OCT, DEC, HEX } base = DEC;
    enum mode mode = U32;
    int base_digits[] = { 2, 8, 10, 16 };
    int has_point = 0;
    int has_neg = 0;
    size_t nsize = strlen(valcpy);

    if (offset[0] == '-') {
      has_neg = 1;
      offset ++;
      nsize --;
    }

    if (strstr(offset, "0b") != NULL) {
      base = BIN;
      offset += 2;
      nsize -= 2;
    } else if (strstr(offset, "0o") != NULL) {
      base = OCT;
      offset += 2;
      nsize -= 2;
    } else if (strstr(offset, "0x") != NULL) {
      base = HEX;
      offset += 2;
      nsize -= 2;
    }

    char *pntpos = strchr(offset, '.');
    if (pntpos != NULL) {
      has_point = 1;
      mode = F32;
    }

    if (strstr(offset, "u32") != NULL) {
      mode = U32;
      nsize -= 3;
      if (has_point) {
        yyerrok;
      }
    } else if (strstr(offset, "u64") != NULL) {
      mode = U64;
      nsize -= 3;
      if (has_point) {
        yyerrok;
      }
    } else if (strstr(offset, "i32") != NULL) {
      mode = I32;
      nsize -= 3;
      if (has_point) {
        yyerrok;
      }
    } else if (strstr(offset, "i64") != NULL) {
      mode = I64;
      nsize -= 3;
      if (has_point) {
        yyerrok;
      }
    } else if (strstr(offset, "f32") != NULL) {
      mode = F32;
      nsize -= 3;
    } else if (strstr(offset, "f64") != NULL) {
      mode = F64;
      nsize -= 3;
    }

    char *nclean = malloc(nsize + 1);
    memset(nclean, 0L, nsize + 1);
    memcpy(nclean, offset, nsize);

    switch (mode) {
    case U8:
    case U16:
    case U32:
      lit.value.u32 = (unsigned int) strtoul(nclean, NULL, base_digits[base]);
      break;
    case U64:
      lit.value.u64 = strtoul(nclean, NULL, base_digits[base]);
      break;
    case I8:
    case I16:
    case I32:
      lit.value.i32 = (int) strtol(nclean, NULL, base_digits[base]);
      if (has_neg) {
        lit.value.i32 = -lit.value.i32;
      }
      break;
    case I64:
      lit.value.i64 = strtol(nclean, NULL, base_digits[base]);
      if (has_neg) {
        lit.value.i64 = -lit.value.i64;
      }
      break;
    case F32:
      lit.value.f32 = atof(nclean);
      if (has_neg) {
        lit.value.f32 = -lit.value.f32;
      }
      break;
    case F64:
      lit.value.f64 = strtod(nclean, NULL);
      if (has_neg) {
        lit.value.f64 = -lit.value.f64;
      }
      break;
    default:
      assert(0); // Unreachable
    }
    free(nclean);
    free(valcpy);

    lit.mode = mode;
    $$ = lit;
  }
  | STRING
  {
    char *unquoted = calloc(strlen(strval)-1, sizeof(char));
    memset(unquoted, 0L, strlen(strval)-1);
    strncpy(unquoted, strval+1, strlen(strval)-2);
    struct typed_value lit;
    lit.value.str = unquoted;
    lit.mode = STR;
    lit.is_ref = 0;
    $$ = lit;
  }

instruction:
  label instruction
  {
    $2->label = $1;
    $$ = $2;
  }
  | iid mode arg1
  {
    if ($1 == -1 || $2 == -1) {
      yyerrok;
    }
    struct instruction *instruction = malloc(sizeof(struct instruction));
    instruction->next = NULL;
    instruction->label = NULL;
    instruction->mnemonic = $1;
    instruction->mode = $2;
    instruction->arg1 = $3;
    instruction->offset = 0L;
    instruction->size = 0L;
    instruction->feed_size = 0L;
    $$ = instruction;
  }
  | iid arg1
  {
    if ($1 == -1) {
      yyerrok;
    }
    struct instruction *instruction = malloc(sizeof(struct instruction));
    instruction->next = NULL;
    instruction->label = NULL;
    instruction->mnemonic = $1;
    instruction->mode = 0;
    instruction->arg1 = $2;
    instruction->offset = 0L;
    instruction->size = 0L;
    instruction->feed_size = 0L;
    $$ = instruction;
  }
  | iid
  {
    if ($1 == -1) {
      yyerrok;
    }
    struct instruction *instruction = malloc(sizeof(struct instruction));
    instruction->next = NULL;
    instruction->label = NULL;
    instruction->mnemonic = $1;
    instruction->mode = 0;
    instruction->arg1 = v_zero;
    instruction->offset = 0L;
    instruction->size = 0L;
    instruction->feed_size = 0L;
    $$ = instruction;
  }
  ;

%%

size_t instruction_calculate_size(struct instruction *instruction) {
  int opcode = instruction->mnemonic;
  int mode = instruction->mode;
  if (opcode == PUSH || opcode == CALL ||
      (opcode >= JNZ && opcode <= JMP) || opcode == SETHDLR ||
      (opcode >= LOAD && opcode <= PSEG)) {
    if (mode == 0x00 || mode == WORD) {
      instruction->size = 4;
    } else if (mode == DWORD) {
      instruction->size = 8;
      instruction->feed_size = 4;
    } else if (mode == QWORD) {
      instruction->size = 12;
      instruction->feed_size = 8;
    }
  } else if (opcode == DATA) {
    switch (mode) {
      case U8:
      case I8:
        instruction->size = 1;
        break;
      case U16:
      case I16:
        instruction->size = 2;
        break;
      case F32:
      case I32:
      case U32:
        instruction->size = 4;
        break;
      case F64:
      case I64:
      case U64:
        instruction->size = 8;
        break;
      case STR:
        instruction->size = strlen(instruction->arg1.value.str) + 1;
        break;
    }
  } else {
    instruction->size = 4;
  }

  return instruction->size;
}

size_t measure_instructions(struct source *src) {
  struct instruction *cur = src->instructions;
  size_t offset = 0L;
  while(cur != NULL) {
    cur->offset = offset;
    offset += instruction_calculate_size(cur);
    cur = cur->next;
  }

  src->output_size = offset + (offset % 4);
  return src->output_size;
}

void collect_label_locations(struct source *src) {
  struct instruction *cur = src->instructions;
  while(cur != NULL) {
    if (cur->label == NULL) {
      cur = cur->next;
      continue;
    }

    struct label_location* loc = malloc(sizeof(struct label_location));
    loc->label = cur->label;
    loc->offset = cur->offset;
    loc->next = NULL;

    struct label_location* curloc = src->label_locations;
    if (curloc == NULL) {
       src->label_locations = loc;
    } else {
       while (curloc->next != NULL) curloc = curloc->next;
       curloc->next = loc;
    }

    cur = cur->next;
  }
}

struct label_location *find_label(struct source *src, const char *wanted) {
  struct label_location *cur = src->label_locations;
  while (cur != NULL) {
    if (strcmp(cur->label, wanted) == 0) {
      return cur;
    }

    cur = cur->next;
  }

  return NULL;
}

void generate_code(struct source *src, char *output) {
  memset(output, 0L, src->output_size);
  struct instruction *instruction = src->instructions;
  while (instruction != NULL) {
    if (instruction->mnemonic == DATA) {
      if (instruction->mode != STR) {
        memcpy(output+instruction->offset, (char *)&instruction->arg1.value, instruction->size);
      } else {
        memcpy(output+instruction->offset, instruction->arg1.value.str, instruction->size);
      }
    } else {
      uint16_t encarg1 = 0;
      struct typed_value arg1 = instruction->arg1;
      if (arg1.is_ref) {
        struct label_location *loc = find_label(src, arg1.value.str);
        if (loc == NULL) {
          fprintf(stderr, "cannot find label: %s\n", arg1.value.str);
        }

        encarg1 = loc->offset;
      } else {
        encarg1 = arg1.value.u16;
      }

      uint32_t i_instruction = ((uint8_t)i_opcodes[instruction->mnemonic] << 24)
        + ((uint8_t)i_modes[instruction->mode] << 16)
        + encarg1;
      char *encoded = (char *)&i_instruction;
      memcpy(output+instruction->offset, encoded, 4);

      switch (instruction->feed_size) {
      case 4:
        encoded = (char *)&instruction->arg1.value.u32;
        memcpy(output+instruction->offset+4, encoded, 4);
        break;
      case 8:
        encoded = (char *)&instruction->arg1.value.u64;
        memcpy(output+instruction->offset+4, encoded, 8);
        break;
      default:
        break;
      }
    }

    // for (size_t i = 0;i<instruction->size;i++) {
    //   printf("%02hhX ", *(output+instruction->offset+i));
    // }
    // printf("\n");
    instruction = instruction->next;
  }
}

int main() {
  struct source src = { .instructions = NULL, .label_locations = NULL, .output_size = 0L};
  int res = yyparse(&src);
  if (res != 0) {
    return res;
  }

  size_t output_size = measure_instructions(&src);
  collect_label_locations(&src);

  char *buffer = malloc(output_size);
  generate_code(&src, buffer);

  fwrite(buffer, sizeof(uint8_t), output_size, stdout);
  return 0;
}

int yyerror(struct source *src, const char *s) {
  fprintf(stderr, "%s\n",s);
  return 0;
}

int yywrap() {
  return 1;
}
