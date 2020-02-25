%{

#include <stdio.h>
#include "y.tab.h"

extern char* strval;
%}
%%
" "       ;
"[ \t]+"  ;
"\n"      { return ENDL; }
"&"       { return AMP; }
":"       { return COLON; }

\"(([^\"]|\\\")*[^\\])?\" {
  strval = yytext;
  return STRING;
}

[a-zA-Z_\$]+[a-zA-Z0-9_]* {
  strval = yytext;
  return ID;
}

\-?(0x|0b|0o)?[0-9ABCDEF]+(\.[0-9ABCDEF]+)?(u32|u64|i32|i64|f32|f64)? {
  strval = yytext;
  return NUMBER;
}
