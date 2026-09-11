#include "ast.h"


Expr *new_expr(ExprKind third_index)
{
	Expr *expression = xcalloc(1, sizeof(*expression));
	expression->kind = third_index;
	return expression;
}

Stmt *new_stmt(StmtKind third_index)
{
	Stmt *statement = xcalloc(1, sizeof(*statement));
	statement->kind = third_index;
	return statement;
}

Decl *new_decl(void)
{
	return xcalloc(1, sizeof(Decl));
}
