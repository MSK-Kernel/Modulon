#ifndef LEXER_H
#define LEXER_H

#include "type.h"

typedef enum
{
	TK_ID,
	TK_NUM,
	TK_STR,
	TK_CHAR,
	TK_OP,
	TK_EOF
} TokKind;

typedef struct
{
	TokKind kind;
	char *v;
	int line;
	int col;
} Token;

typedef struct
{
	Token *a;
	size_t n;
	size_t cap;
	const char *file;
	const char *source;
} Tokens;

Tokens lex_source(const char *text, const char *file);

#endif
