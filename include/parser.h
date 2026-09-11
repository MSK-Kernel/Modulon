#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct
{
	Tokens *ts;
	size_t p;
	Program *prog;
} Parser;

void parse_program(Tokens *token_stream, Program *prog);
StructMember *find_struct_member(CType *type, const char *name);
bool eval_const_expr(Expr *expression, long *value);

#endif
