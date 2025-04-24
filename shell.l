/*
 * shell.l: lexical analyzer for shell
 * Extended to support special characters and contiguous command tokens.
 */

%{
#include <string.h>
#include "y.tab.h"
%}

%%

\n {
	return NEWLINE;
}

[ \t]+ {
	/* Discard spaces and tabs */
}

">>" {
	return GREATGREAT;
}

">" {
	return GREAT;
}

"<" {
	return LESS;
}

"&" {
	return AMPERSAND;
}

"|" {
	return PIPE;
}

[^ \t\n><&|]+ {
	yylval.string_val = strdup(yytext);
	return WORD;
}

"." { return NOTOKEN; }


%%


