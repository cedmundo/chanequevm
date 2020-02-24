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
  strval = strdup(yytext);
  return STRING;
}

\-?(0x|0b|0o)?[0-9ABCDEF]+(\.[0-9ABCDEF]+)? {
  strval = strdup(yytext);
  return NUMBER;
}

[a-zA-Z_\$]+[a-zA-Z0-9_]* {
  strval = strdup(yytext);
  return ID;
}
