#include "parser.h"


static Token *ptok(Parser *parser)
{
	return &parser->ts->a[parser->p];
}

static bool peq(Parser *parser, const char *string)
{
	return !strcmp(ptok(parser)->v, string);
}

static bool paccept(Parser *parser, const char *string)
{
	if(peq(parser, string)) {
		parser->p++;
		return true;
	}
	return false;
}

static void perr(Parser *parser, const char *fmt, ...)
{
	va_list argument_list;
	Token *token = ptok(parser);
	fprintf(stderr, "aneoc: %s:%d:%d: ", parser->ts->file, token->line, token->col);
	va_start(argument_list, fmt);
	vfprintf(stderr, fmt, argument_list);
	va_end(argument_list);
	fprintf(stderr, "; got '%s'\n", token->v);
	exit(1);
}

static void pexpect(Parser *parser, const char *string)
{
	if(!paccept(parser, string))
		perr(parser, "expected '%s'", string);
}

static char *pexpect_id(Parser *parser, bool optional)
{
	if(ptok(parser)->kind == TK_ID)
		return parser->ts->a[parser->p++].v;
	if(optional)
		return xstrdup("");
	perr(parser, "expected identifier");
	return NULL;
}

static CType *find_alias(Program *prog, const char *name)
{
	size_t index;
	for(index = 0; index < prog->naliases; index++)
		if(!strcmp(prog->aliases[index].name, name))
			return prog->aliases[index].type;
	return NULL;
}

static bool same_type(CType *array, CType *base_type)
{
	if(array == base_type)
		return true;
	if(!array || !base_type || array->kind != base_type->kind || array->count != base_type->count)
		return false;
	if(array->kind == TY_PTR || array->kind == TY_ARRAY)
		return same_type(array->base, base_type->base);
	return true;
}

static void add_alias(Parser *parser, char *name, CType *type)
{
	Program *prog = parser->prog;
	CType *old = find_alias(prog, name);
	if(old) {
		if(same_type(old, type))
			return;
		perr(parser, "conflicting typedef '%s'", name);
	}
	ARR_GROW(prog->aliases, prog->naliases, prog->capaliases, TypeAlias);
	prog->aliases[prog->naliases].name = name;
	prog->aliases[prog->naliases].type = type;
	prog->naliases++;
}

static CType *find_struct_tag(Program *prog, const char *name)
{
	size_t index;
	for(index = 0; index < prog->ntags; index++)
		if(!strcmp(prog->tags[index].name, name))
			return prog->tags[index].type;
	return NULL;
}

static void add_struct_tag(Parser *parser, char *name, CType *type)
{
	CType *old;
	if(!name || !*name)
		return;
	old = find_struct_tag(parser->prog, name);
	if(old && old != type)
		perr(parser, "conflicting struct tag '%s'", name);
	if(old)
		return;
	ARR_GROW(parser->prog->tags, parser->prog->ntags, parser->prog->captags, StructTag);
	parser->prog->tags[parser->prog->ntags].name = name;
	parser->prog->tags[parser->prog->ntags].type = type;
	parser->prog->ntags++;
}

StructMember *find_struct_member(CType *type, const char *name)
{
	static StructMember xevent_type_member = {"type", &T_INT, 0};
	size_t index;
	if(!type)
		return NULL;
	if(type->kind == TY_XEVENT)
		return !strcmp(name, "type") ? &xevent_type_member : NULL;
	if(type->kind != TY_STRUCT)
		return NULL;
	for(index = 0; index < type->nmembers; index++)
		if(!strcmp(type->members[index].name, name))
			return &type->members[index];
	return NULL;
}
static Declarator parse_declarator(Parser *p, CType *base, bool unnamed);
static Expr *parse_expr(Parser *p, int minprec);
bool eval_const_expr(Expr *e, long *value);

static bool type_start(Parser *parser)
{
	const char *character = ptok(parser)->v;
	if(find_alias(parser->prog, character))
		return true;
	return !strcmp(character, "void") || !strcmp(character, "_Bool") ||
	       !strcmp(character, "bool") || !strcmp(character, "string") || !strcmp(character, "char") ||
	       !strcmp(character, "short") || !strcmp(character, "int") ||
	       !strcmp(character, "long") || !strcmp(character, "float") ||
	       !strcmp(character, "double") || !strcmp(character, "signed") ||
	       !strcmp(character, "unsigned") || !strcmp(character, "const") ||
	       !strcmp(character, "volatile") || !strcmp(character, "restrict") ||
	       !strcmp(character, "struct") ||
	       !strcmp(character, "FILE") || !strcmp(character, "Display") ||
	       !strcmp(character, "Window") || !strcmp(character, "GC") ||
	       !strcmp(character, "Font") || !strcmp(character, "XEvent") ||
	       !strcmp(character, "va_list") || !strcmp(character, "size_t") ||
	       !strcmp(character, "ssize_t") || !strcmp(character, "ptrdiff_t") ||
	       !strcmp(character, "intptr_t") || !strcmp(character, "uintptr_t") ||
	       !strcmp(character, "intmax_t") || !strcmp(character, "uintmax_t") ||
	       !strcmp(character, "int8_t") || !strcmp(character, "int16_t") ||
	       !strcmp(character, "int32_t") || !strcmp(character, "int64_t") ||
	       !strcmp(character, "uint8_t") || !strcmp(character, "uint16_t") ||
	       !strcmp(character, "uint32_t") || !strcmp(character, "uint64_t") ||
	       !strcmp(character, "int_least8_t") || !strcmp(character, "int_least16_t") ||
	       !strcmp(character, "int_least32_t") || !strcmp(character, "int_least64_t") ||
	       !strcmp(character, "uint_least8_t") || !strcmp(character, "uint_least16_t") ||
	       !strcmp(character, "uint_least32_t") || !strcmp(character, "uint_least64_t") ||
	       !strcmp(character, "int_fast8_t") || !strcmp(character, "int_fast16_t") ||
	       !strcmp(character, "int_fast32_t") || !strcmp(character, "int_fast64_t") ||
	       !strcmp(character, "uint_fast8_t") || !strcmp(character, "uint_fast16_t") ||
	       !strcmp(character, "uint_fast32_t") || !strcmp(character, "uint_fast64_t");
}

static bool parse_gnu_attributes(Parser *parser)
{
	bool packed = false;
	while(peq(parser, "__attribute__") || peq(parser, "__attribute")) {
		int depth = 0;
		parser->p++;
		pexpect(parser, "(");
		depth = 1;
		while(depth > 0) {
			if(ptok(parser)->kind == TK_EOF)
				perr(parser, "unterminated __attribute__");
			if(peq(parser, "packed") || peq(parser, "__packed__"))
				packed = true;
			if(peq(parser, "("))
				depth++;
			else if(peq(parser, ")"))
				depth--;
			parser->p++;
		}
	}
	return packed;
}

static void finish_struct_layout(CType *type, bool packed)
{
	long offset = 0;
	long maximum = 1;
	size_t index;
	type->packed = packed;
	for(index = 0; index < type->nmembers; index++) {
		long alignment = packed ? 1 : type_align(type->members[index].type);
		long size = type_size(type->members[index].type);
		if(alignment < 1)
			alignment = 1;
		offset = (offset + alignment - 1) / alignment * alignment;
		type->members[index].offset = offset;
		offset += size;
		if(alignment > maximum)
			maximum = alignment;
	}
	type->align = packed ? 1 : maximum;
	type->size = packed ? offset : (offset + maximum - 1) / maximum * maximum;
}

static CType *parse_type(Parser *parser)
{
	const char *character;
	CType *alias;
	bool is_unsigned = false;
	bool is_signed = false;
	while(peq(parser, "const") || peq(parser, "volatile") || peq(parser, "restrict"))
		parser->p++;
	if(paccept(parser, "struct")) {
		char *tag = NULL;
		CType *type;
		if(ptok(parser)->kind == TK_ID && !peq(parser, "{"))
			tag = pexpect_id(parser, false);
		if(!paccept(parser, "{")) {
			type = find_struct_tag(parser->prog, tag ? tag : "");
			if(!type)
				perr(parser, "unknown struct '%s'", tag ? tag : "");
			return type;
		}
		type = new_type(TY_STRUCT, NULL, 0, tag ? tag : "<anonymous>");
		add_struct_tag(parser, tag, type);
		while(!paccept(parser, "}")) {
			CType *member_base = parse_type(parser);
			Declarator member = parse_declarator(parser, member_base, false);
			if(member.function)
				perr(parser, "struct member cannot be a function");
			pexpect(parser, ";");
			ARR_GROW(type->members, type->nmembers, type->capmembers, StructMember);
			type->members[type->nmembers].name = member.name;
			type->members[type->nmembers].type = member.type;
			type->members[type->nmembers].offset = 0;
			type->nmembers++;
		}
		finish_struct_layout(type, parse_gnu_attributes(parser));
		return type;
	}
	if(paccept(parser, "unsigned"))
		is_unsigned = true;
	else if(paccept(parser, "signed"))
		is_signed = true;
	if(paccept(parser, "long")) {
		bool second_long = paccept(parser, "long");
		if(!second_long && paccept(parser, "double")) {
			if(is_unsigned || is_signed)
				perr(parser, "invalid signedness for long double");
			return &T_LDOUBLE;
		}
		paccept(parser, "int");
		if(is_unsigned)
			return second_long ? &T_U64 : &T_ULONG;
		return second_long ? &T_LLONG : &T_LONG;
	}
	if(paccept(parser, "short")) {
		if(paccept(parser, "int"))
			return is_unsigned ? &T_U16 : &T_SHORT;
		return is_unsigned ? &T_U16 : &T_SHORT;
	}
	if(paccept(parser, "string")) {
		if(is_unsigned || is_signed)
			perr(parser, "invalid signedness for string");
		return ptr_to(&T_CHAR);
	}
	if(paccept(parser, "char"))
		return is_unsigned ? &T_U8 : &T_CHAR;
	if(paccept(parser, "int"))
		return is_unsigned ? &T_U32 : &T_INT;
	if(is_unsigned)
		return &T_U32;
	if(is_signed)
		return &T_INT;
	if(paccept(parser, "float")) {
		if(is_unsigned || is_signed)
			perr(parser, "invalid signedness for float");
		return &T_FLOAT;
	}
	if(paccept(parser, "double")) {
		if(is_unsigned || is_signed)
			perr(parser, "invalid signedness for double");
		return &T_DOUBLE;
	}
	if(is_unsigned || is_signed)
		perr(parser, "expected integer type");
	character = ptok(parser)->v;
	alias = find_alias(parser->prog, character);
	if(alias) {
		parser->p++;
		return alias;
	}
	if(!strcmp(character, "void")) {
		parser->p++;
		return &T_VOID;
	}
	if(!strcmp(character, "_Bool") || !strcmp(character, "bool")) {
		parser->p++;
		return &T_BOOL;
	}
	if(!strcmp(character, "int8_t") || !strcmp(character, "int_least8_t") ||
	   !strcmp(character, "int_fast8_t")) {
		parser->p++;
		return &T_CHAR;
	}
	if(!strcmp(character, "uint8_t") || !strcmp(character, "uint_least8_t") ||
	   !strcmp(character, "uint_fast8_t")) {
		parser->p++;
		return &T_U8;
	}
	if(!strcmp(character, "int16_t") || !strcmp(character, "int_least16_t") ||
	   !strcmp(character, "int_fast16_t")) {
		parser->p++;
		return &T_SHORT;
	}
	if(!strcmp(character, "uint16_t") || !strcmp(character, "uint_least16_t") ||
	   !strcmp(character, "uint_fast16_t")) {
		parser->p++;
		return &T_U16;
	}
	if(!strcmp(character, "int32_t") || !strcmp(character, "int_least32_t") ||
	   !strcmp(character, "int_fast32_t")) {
		parser->p++;
		return &T_INT;
	}
	if(!strcmp(character, "uint32_t") || !strcmp(character, "uint_least32_t") ||
	   !strcmp(character, "uint_fast32_t")) {
		parser->p++;
		return &T_U32;
	}
	if(!strcmp(character, "int64_t") || !strcmp(character, "int_least64_t") ||
	   !strcmp(character, "int_fast64_t")) {
		parser->p++;
		return &T_I64;
	}
	if(!strcmp(character, "uint64_t") || !strcmp(character, "uint_least64_t") ||
	   !strcmp(character, "uint_fast64_t")) {
		parser->p++;
		return &T_U64;
	}
	if(!strcmp(character, "size_t") || !strcmp(character, "uintptr_t")) {
		parser->p++;
		return type_get_target() == TARGET_I386 ? &T_U32 : &T_U64;
	}
	if(!strcmp(character, "ssize_t") || !strcmp(character, "ptrdiff_t") ||
	   !strcmp(character, "intptr_t")) {
		parser->p++;
		return type_get_target() == TARGET_I386 ? &T_INT : &T_I64;
	}
	if(!strcmp(character, "intmax_t")) {
		parser->p++;
		return &T_I64;
	}
	if(!strcmp(character, "uintmax_t")) {
		parser->p++;
		return &T_U64;
	}
	if(!strcmp(character, "FILE")) {
		parser->p++;
		return &T_FILE;
	}
	if(!strcmp(character, "Display")) {
		parser->p++;
		return &T_DISPLAY;
	}
	if(!strcmp(character, "Window")) {
		parser->p++;
		return type_get_target() == TARGET_I386 ? &T_U32 : &T_U64;
	}
	if(!strcmp(character, "GC")) {
		parser->p++;
		return ptr_to(&T_VOID);
	}
	if(!strcmp(character, "Font")) {
		parser->p++;
		return type_get_target() == TARGET_I386 ? &T_U32 : &T_U64;
	}
	if(!strcmp(character, "XEvent")) {
		parser->p++;
		return &T_XEVENT;
	}
	if(!strcmp(character, "va_list")) {
		parser->p++;
		return &T_VALIST;
	}
	perr(parser, "expected type");
	return NULL;
}

static Declarator parse_declarator(Parser *parser, CType *base, bool unnamed)
{
	Declarator d = {0};
	CType *type_1 = base;
	while(paccept(parser, "*"))
		type_1 = ptr_to(type_1);
	d.name = pexpect_id(parser, unnamed);
	d.type = type_1;
	if(paccept(parser, "(")) {
		d.function = true;
		if(paccept(parser, ")"))
			return d;
		if(peq(parser, "void") &&
		   !strcmp(parser->ts->a[parser->p + 1].v, ")"))
		{
			parser->p += 2;
			return d;
		}
		for(;;) {
			CType *parsed_type;
			char *parameter_name;
			if(paccept(parser, "...")) {
				d.variadic = true;
				pexpect(parser, ")");
				break;
			}
			parsed_type = parse_type(parser);
			while(paccept(parser, "*"))
				parsed_type = ptr_to(parsed_type);
			parameter_name = pexpect_id(parser, true);
			if(!*parameter_name) {
				char temporary_buffer[32];
				snprintf(temporary_buffer, sizeof(temporary_buffer), "__arg%zu", d.nparams);
				parameter_name = xstrdup(temporary_buffer);
			}
			if(paccept(parser, "[")) {
				if(ptok(parser)->kind == TK_NUM)
					parser->p++;
				pexpect(parser, "]");
				parsed_type = ptr_to(parsed_type);
			}
			ARR_GROW(d.params, d.nparams, d.capparams, Param);
			d.params[d.nparams].name = parameter_name;
			d.params[d.nparams].type = parsed_type;
			d.nparams++;
			if(paccept(parser, ")"))
				break;
			pexpect(parser, ",");
		}
		return d;
	}
	while(paccept(parser, "[")) {
		Expr *bound;
		long count;
		if(paccept(parser, "]")) {
			type_1 = array_of(type_1, 0);
			d.type = type_1;
			continue;
		}
		bound = parse_expr(parser, 1);
		pexpect(parser, "]");
		if(eval_const_expr(bound, &count)) {
			if(count < 0)
				perr(parser, "array length cannot be negative");
			type_1 = array_of(type_1, count);
		} else if(bound->kind == EX_ID)
		{
			type_1 = vla_of(type_1, bound->str);
		} else {
			perr(parser, "array length must be a constant expression or identifier");
		}
		d.type = type_1;
	}
	return d;
}

static char decode_escape(const char **pointer_pointer)
{
	const char *position = *pointer_pointer;
	char character = *position++;
	if(character != '\\') {
		*pointer_pointer = position;
		return character;
	}
	character = *position++;
	switch(character) {
	case 'a':
		character = '\a';
		break;
	case 'b':
		character = '\b';
		break;
	case 'f':
		character = '\f';
		break;
	case 'n':
		character = '\n';
		break;
	case 'r':
		character = '\r';
		break;
	case 't':
		character = '\t';
		break;
	case 'v':
		character = '\v';
		break;
	case '0':
		character = '\0';
		break;
	case '\\':
		character = '\\';
		break;
	case '\'':
		character = '\'';
		break;
	case '"':
		character = '"';
		break;
	default:
		break;
	}
	*pointer_pointer = position;
	return character;
}

static char *decode_string(const char *raw)
{
	size_t length = strlen(raw), index = 1;
	Buf buffer = {0};
	while(index + 1 < length) {
		const char *position = raw + index;
		char character = decode_escape(&position);
		bputn(&buffer, &character, 1);
		index = (size_t)(position - raw);
	}
	if(!buffer.s)
		buffer.s = xstrdup("");
	return buffer.s;
}

static Expr *parse_primary(Parser *parser)
{
	Token *token = ptok(parser);
	Expr *expression;
	if(paccept(parser, "(")) {
		expression = parse_expr(parser, 1);
		pexpect(parser, ")");
		return expression;
	}
	if(token->kind == TK_NUM) {
		bool floating = strchr(token->v, '.') != NULL;
		const char *number = token->v;
		if(!floating && !(number[0] == '0' && (number[1] == 'x' || number[1] == 'X')) &&
		   (strchr(number, 'e') || strchr(number, 'E')))
			floating = true;
		if(floating) {
			expression = new_expr(EX_NUM);
			expression->fnum = strtod(token->v, NULL);
			expression->type = &T_DOUBLE;
			parser->p++;
			return expression;
		}
		char *cursor_1 = xstrdup(token->v), *x_value = cursor_1;
		char *suffix = NULL;
		unsigned long long value;
		bool wide = false;
		bool uns = false;
		while(*x_value) {
			if(strchr("uUlL", *x_value)) {
				suffix = x_value;
				*x_value = 0;
				break;
			}
			x_value++;
		}
		if(suffix) {
			const char *result = token->v + (suffix - cursor_1);
			while(*result) {
				if(*result == 'u' || *result == 'U')
					uns = true;
				if((*result == 'l' || *result == 'L') &&
				   (result[1] == 'l' || result[1] == 'L'))
					wide = true;
				result++;
			}
		}
		value = strtoull(cursor_1, NULL, 0);
		free(cursor_1);
		parser->p++;
		expression = new_expr(EX_NUM);
		expression->num = value;
		expression->type = (wide || value > 0xffffffffULL) ? &T_U64 : (uns ? &T_U32 : &T_INT);
		return expression;
	}
	if(token->kind == TK_STR) {
		Buf joined = {0};
		while(ptok(parser)->kind == TK_STR) {
			char *part = decode_string(ptok(parser)->v);
			bputs(&joined, part);
			free(part);
			parser->p++;
		}
		expression = new_expr(EX_STR);
		expression->str = joined.s ? joined.s : xstrdup("");
		expression->type = ptr_to(&T_CHAR);
		return expression;
	}
	if(token->kind == TK_CHAR) {
		const char *cursor_1 = token->v + 1;
		char character = decode_escape(&cursor_1);
		parser->p++;
		expression = new_expr(EX_NUM);
		expression->num = (unsigned char)character;
		expression->type = &T_INT;
		return expression;
	}
	if(paccept(parser, "typeof")) {
		expression = new_expr(EX_TYPEOF);
		pexpect(parser, "(");
		if(type_start(parser))
			expression->sizeof_type = parse_type(parser);
		else
			expression->left = parse_expr(parser, 1);
		pexpect(parser, ")");
		return expression;
	}
	if(type_start(parser)) {
		expression = new_expr(EX_TYPE);
		expression->type = parse_type(parser);
		while(paccept(parser, "*"))
			expression->type = ptr_to(expression->type);
		return expression;
	}
	if(token->kind == TK_ID) {
		parser->p++;
		expression = new_expr(EX_ID);
		expression->str = token->v;
		return expression;
	}
	perr(parser, "expected expression");
	return NULL;
}

static Expr *parse_postfix(Parser *parser)
{
	Expr *expression = parse_primary(parser);
	for(;;) {
		if(paccept(parser, "(")) {
			Expr *call_expression = new_expr(EX_CALL);
			call_expression->left = expression;
			if(!paccept(parser, ")"))
				for(;;) {
					Expr *expression_1 = parse_expr(parser, 1);
					ARR_GROW(call_expression->args, call_expression->nargs, call_expression->capargs, Expr *);
					call_expression->args[call_expression->nargs++] = expression_1;
					if(paccept(parser, ")"))
						break;
					pexpect(parser, ",");
				}
			expression = call_expression;
			continue;
		}
		if(paccept(parser, "[")) {
			Expr *expression_2 = new_expr(EX_INDEX);
			expression_2->left = expression;
			expression_2->right = parse_expr(parser, 1);
			pexpect(parser, "]");
			expression = expression_2;
			continue;
		}
		if(paccept(parser, ".")) {
			Expr *expression_2 = new_expr(EX_MEMBER);
			expression_2->left = expression;
			expression_2->str = pexpect_id(parser, false);
			expression = expression_2;
			continue;
		}
		if(paccept(parser, "->")) {
			Expr *expression_2 = new_expr(EX_PTRMEMBER);
			expression_2->left = expression;
			expression_2->str = pexpect_id(parser, false);
			expression = expression_2;
			continue;
		}
		if(paccept(parser, "++")) {
			Expr *expression_2 = new_expr(EX_UNARY);
			expression_2->op = "post++";
			expression_2->left = expression;
			expression = expression_2;
			continue;
		}
		if(paccept(parser, "--")) {
			Expr *expression_2 = new_expr(EX_UNARY);
			expression_2->op = "post--";
			expression_2->left = expression;
			expression = expression_2;
			continue;
		}
		break;
	}
	return expression;
}

static Expr *parse_unary(Parser *parser)
{
	if(peq(parser, "(")) {
		size_t save = parser->p;
		CType *cast_type;
		Expr *expression_1;
		parser->p++;
		if(type_start(parser)) {
			cast_type = parse_type(parser);
			while(paccept(parser, "*"))
				cast_type = ptr_to(cast_type);
			if(paccept(parser, ")")) {
				expression_1 = new_expr(EX_UNARY);
				expression_1->op = "cast";
				expression_1->type = cast_type;
				expression_1->left = parse_unary(parser);
				return expression_1;
			}
		}
		parser->p = save;
	}
	if(peq(parser, "++") || peq(parser, "--")) {
		Expr *expression_1 = new_expr(EX_UNARY);
		expression_1->op = ptok(parser)->v;
		parser->p++;
		expression_1->left = parse_unary(parser);
		return expression_1;
	}
	if(peq(parser, "!") || peq(parser, "~") || peq(parser, "-") || peq(parser, "+") || peq(parser, "&") || peq(parser, "*")) {
		Expr *expression_1 = new_expr(EX_UNARY);
		expression_1->op = ptok(parser)->v;
		parser->p++;
		expression_1->left = parse_unary(parser);
		return expression_1;
	}
	if(paccept(parser, "sizeof")) {
		Expr *expression_1 = new_expr(EX_SIZEOF);
		pexpect(parser, "(");
		if(type_start(parser)) {
			expression_1->sizeof_type = parse_type(parser);
			while(paccept(parser, "*"))
				expression_1->sizeof_type = ptr_to(expression_1->sizeof_type);
		} else {
			expression_1->left = parse_expr(parser, 1);
		}
		pexpect(parser, ")");
		return expression_1;
	}
	return parse_postfix(parser);
}

static int prec(const char *string)
{
	if(!strcmp(string, "=") || !strcmp(string, "+=") || !strcmp(string, "-=") ||
	   !strcmp(string, "*=") || !strcmp(string, "/=") || !strcmp(string, "%=") ||
	   !strcmp(string, "&=") || !strcmp(string, "|=") || !strcmp(string, "^=") ||
	   !strcmp(string, "<<=") || !strcmp(string, ">>="))
		return 1;
	if(!strcmp(string, "||"))
		return 2;
	if(!strcmp(string, "&&"))
		return 3;
	if(!strcmp(string, "|"))
		return 4;
	if(!strcmp(string, "^"))
		return 5;
	if(!strcmp(string, "&"))
		return 6;
	if(!strcmp(string, "==") || !strcmp(string, "!="))
		return 7;
	if(!strcmp(string, "<") || !strcmp(string, "<=") || !strcmp(string, ">") || !strcmp(string, ">="))
		return 8;
	if(!strcmp(string, "<<") || !strcmp(string, ">>"))
		return 9;
	if(!strcmp(string, "+") || !strcmp(string, "-"))
		return 10;
	if(!strcmp(string, "*") || !strcmp(string, "/") || !strcmp(string, "%"))
		return 11;
	return 0;
}

static Expr *parse_expr(Parser *parser, int minprec)
{
	Expr *lhs = parse_unary(parser);
	for(;;) {
		int precedence = prec(ptok(parser)->v);
		char *operator;
		Expr *rhs, *expression;
		bool right;
		if(precedence < minprec)
			break;
		operator = ptok(parser)->v;
		parser->p++;
		right = (!strcmp(operator, "=") || !strcmp(operator, "+=") || !strcmp(operator, "-=") ||
			 !strcmp(operator, "*=") || !strcmp(operator, "/=") || !strcmp(operator, "%=") ||
			 !strcmp(operator, "&=") || !strcmp(operator, "|=") || !strcmp(operator, "^=") ||
			 !strcmp(operator, "<<=") || !strcmp(operator, ">>="));
		rhs = parse_expr(parser, right ? precedence : precedence + 1);
		expression = new_expr(EX_BINARY);
		expression->op = operator;
		expression->left = lhs;
		expression->right = rhs;
		lhs = expression;
	}
	return lhs;
}

static Expr *parse_initializer(Parser *parser)
{
	Expr *expression;
	if(!paccept(parser, "{"))
		return parse_expr(parser, 1);
	expression = new_expr(EX_INITLIST);
	if(paccept(parser, "}"))
		return expression;
	for(;;) {
		Expr *item = parse_initializer(parser);
		ARR_GROW(expression->args, expression->nargs, expression->capargs, Expr *);
		expression->args[expression->nargs++] = item;
		if(paccept(parser, "}"))
			break;
		pexpect(parser, ",");
		if(paccept(parser, "}"))
			break;
	}
	return expression;
}

bool eval_const_expr(Expr *expression, long *value)
{
	long array, boundary;
	if(!expression)
		return false;
	if(expression->kind == EX_NUM) {
		*value = (long)expression->num;
		return true;
	}
	if(expression->kind == EX_ID && !strcmp(expression->str, "NULL")) {
		*value = 0;
		return true;
	}
	if(expression->kind == EX_UNARY && eval_const_expr(expression->left, &array)) {
		if(!strcmp(expression->op, "+") || !strcmp(expression->op, "cast"))
			*value = array;
		else if(!strcmp(expression->op, "-"))
			*value = -array;
		else if(!strcmp(expression->op, "~"))
			*value = ~array;
		else if(!strcmp(expression->op, "!"))
			*value = !array;
		else
			return false;
		return true;
	}
	if(expression->kind != EX_BINARY || !eval_const_expr(expression->left, &array) ||
	   !eval_const_expr(expression->right, &boundary))
		return false;
	if(!strcmp(expression->op, "+"))
		*value = array + boundary;
	else if(!strcmp(expression->op, "-"))
		*value = array - boundary;
	else if(!strcmp(expression->op, "*"))
		*value = array * boundary;
	else if(!strcmp(expression->op, "/")) {
		if(!boundary)
			return false;
		*value = array / boundary;
	} else if(!strcmp(expression->op, "%"))
	{
		if(!boundary)
			return false;
		*value = array % boundary;
	} else if(!strcmp(expression->op, "<<"))
		*value = array << boundary;
	else if(!strcmp(expression->op, ">>"))
		*value = array >> boundary;
	else if(!strcmp(expression->op, "&"))
		*value = array & boundary;
	else if(!strcmp(expression->op, "|"))
		*value = array | boundary;
	else if(!strcmp(expression->op, "^"))
		*value = array ^ boundary;
	else
		return false;
	return true;
}

static void infer_array_bound(Parser *parser, CType *type, Expr *initializer)
{
	if(!type || type->kind != TY_ARRAY || type->count != 0)
		return;
	if(!initializer)
		return;
	if(initializer->kind == EX_INITLIST)
		type->count = (long)initializer->nargs;
	else if(initializer->kind == EX_STR &&
		(type->base->kind == TY_CHAR || type->base->kind == TY_U8))
		type->count = (long)strlen(initializer->str) + 1;
	else
		perr(parser, "array with omitted length requires an initializer");
}
static Stmt *parse_stmt(Parser *p);

static Stmt *parse_block(Parser *parser)
{
	Stmt *statement = new_stmt(ST_BLOCK);
	pexpect(parser, "{");
	while(!paccept(parser, "}")) {
		Stmt *statement_1;
		if(paccept(parser, "var")) {
			Decl *declaration = new_decl();
			declaration->name = pexpect_id(parser, false);
			if(!paccept(parser, "="))
				perr(parser, "var declaration requires an initializer");
			declaration->init = parse_initializer(parser);
			declaration->is_var = true;
			statement_1 = new_stmt(ST_DECL);
			statement_1->decl = declaration;
			ARR_GROW(statement->children, statement->nchildren, statement->capchildren, Stmt *);
			statement->children[statement->nchildren++] = statement_1;
			pexpect(parser, ";");
			continue;
		}
		if(type_start(parser)) {
			CType *base_type = parse_type(parser);
			for(;;) {
				Declarator q = parse_declarator(parser, base_type, false);
				Decl *declaration = new_decl();
				if(q.function)
					perr(parser, "nested function unsupported");
				declaration->name = q.name;
				declaration->type = q.type;
				if(paccept(parser, "="))
					declaration->init = parse_initializer(parser);
				infer_array_bound(parser, declaration->type, declaration->init);
				statement_1 = new_stmt(ST_DECL);
				statement_1->decl = declaration;
				ARR_GROW(statement->children, statement->nchildren, statement->capchildren, Stmt *);
				statement->children[statement->nchildren++] = statement_1;
				if(!paccept(parser, ","))
					break;
			}
			pexpect(parser, ";");
			continue;
		}
		statement_1 = parse_stmt(parser);
		ARR_GROW(statement->children, statement->nchildren, statement->capchildren, Stmt *);
		statement->children[statement->nchildren++] = statement_1;
	}
	return statement;
}

static Stmt *parse_stmt(Parser *parser)
{
	Stmt *statement;
	if(peq(parser, "{"))
		return parse_block(parser);
	if(paccept(parser, "if")) {
		statement = new_stmt(ST_IF);
		pexpect(parser, "(");
		statement->cond = parse_expr(parser, 1);
		pexpect(parser, ")");
		statement->yes = parse_stmt(parser);
		if(paccept(parser, "else"))
			statement->no = parse_stmt(parser);
		return statement;
	}
	if(paccept(parser, "while")) {
		statement = new_stmt(ST_WHILE);
		pexpect(parser, "(");
		statement->cond = parse_expr(parser, 1);
		pexpect(parser, ")");
		statement->body = parse_stmt(parser);
		return statement;
	}
	if(paccept(parser, "for")) {
		statement = new_stmt(ST_FOR);
		pexpect(parser, "(");
		if(!paccept(parser, ";")) {
			if(paccept(parser, "var")) {
				Decl *declaration = new_decl();
				declaration->name = pexpect_id(parser, false);
				if(!paccept(parser, "="))
					perr(parser, "var declaration requires an initializer");
				declaration->init = parse_initializer(parser);
				declaration->is_var = true;
				pexpect(parser, ";");
				statement->init = new_stmt(ST_DECL);
				statement->init->decl = declaration;
			} else if(type_start(parser)) {
				CType *base_type = parse_type(parser);
				Declarator q = parse_declarator(parser, base_type, false);
				Decl *declaration = new_decl();
				if(q.function)
					perr(parser, "function declaration in for initializer unsupported");
				declaration->name = q.name;
				declaration->type = q.type;
				if(paccept(parser, "="))
					declaration->init = parse_initializer(parser);
				infer_array_bound(parser, declaration->type, declaration->init);
				pexpect(parser, ";");
				statement->init = new_stmt(ST_DECL);
				statement->init->decl = declaration;
			} else {
				statement->init = new_stmt(ST_EXPR);
				statement->init->expr = parse_expr(parser, 1);
				pexpect(parser, ";");
			}
		}
		if(!paccept(parser, ";")) {
			statement->cond = parse_expr(parser, 1);
			pexpect(parser, ";");
		}
		if(!paccept(parser, ")")) {
			statement->post = parse_expr(parser, 1);
			pexpect(parser, ")");
		}
		statement->body = parse_stmt(parser);
		return statement;
	}
	if(paccept(parser, "switch")) {
		statement = new_stmt(ST_SWITCH);
		pexpect(parser, "(");
		statement->cond = parse_expr(parser, 1);
		pexpect(parser, ")");
		statement->body = parse_stmt(parser);
		return statement;
	}
	if(paccept(parser, "case")) {
		statement = new_stmt(ST_CASE);
		statement->expr = parse_expr(parser, 1);
		pexpect(parser, ":");
		return statement;
	}
	if(paccept(parser, "default")) {
		statement = new_stmt(ST_DEFAULT);
		pexpect(parser, ":");
		return statement;
	}
	if(paccept(parser, "return")) {
		statement = new_stmt(ST_RETURN);
		if(!paccept(parser, ";")) {
			statement->expr = parse_expr(parser, 1);
			pexpect(parser, ";");
		}
		return statement;
	}
	if(paccept(parser, "break")) {
		pexpect(parser, ";");
		return new_stmt(ST_BREAK);
	}
	if(paccept(parser, "continue")) {
		pexpect(parser, ";");
		return new_stmt(ST_CONTINUE);
	}
	if(paccept(parser, "asm") || paccept(parser, "__asm__")) {
		Buf text = {0};
		statement = new_stmt(ST_ASM);
		paccept(parser, "volatile");
		paccept(parser, "__volatile__");
		pexpect(parser, "(");
		if(ptok(parser)->kind != TK_STR)
			perr(parser, "inline asm requires a string literal");
		while(ptok(parser)->kind == TK_STR) {
			char *part = decode_string(ptok(parser)->v);
			bputs(&text, part);
			free(part);
			parser->p++;
		}
		if(paccept(parser, ":")) {
			if(!peq(parser, ")") && !peq(parser, ":")) {
				for(;;) {
					char *constraint;
					Expr *output;
					if(ptok(parser)->kind != TK_STR)
						perr(parser, "asm output constraint must be a string");
					constraint = decode_string(ptok(parser)->v);
					parser->p++;
					pexpect(parser, "(");
					output = parse_expr(parser, 1);
					pexpect(parser, ")");
					ARR_GROW(statement->asm_constraints, statement->nasm_outputs, statement->capasm_constraints, char *);
					statement->asm_constraints[statement->nasm_outputs] = constraint;
					ARR_GROW(statement->asm_outputs, statement->nasm_outputs, statement->capasm_outputs, Expr *);
					statement->asm_outputs[statement->nasm_outputs] = output;
					statement->nasm_outputs++;
					if(!paccept(parser, ","))
						break;
				}
			}
			//Inputs and clobbers are parsed only when empty for now.
			if(paccept(parser, ":")) {
				if(!peq(parser, ")") && !peq(parser, ":"))
					perr(parser, "asm inputs are not supported yet");
				paccept(parser, ":");
			}
		}
		pexpect(parser, ")");
		pexpect(parser, ";");
		statement->asm_text = text.s ? text.s : xstrdup("");
		return statement;
	}
	if(paccept(parser, ";"))
		return new_stmt(ST_EMPTY);
	statement = new_stmt(ST_EXPR);
	statement->expr = parse_expr(parser, 1);
	pexpect(parser, ";");
	return statement;
}

void parse_program(Tokens *token_stream, Program *prog)
{
	Parser p = {token_stream, 0, prog};
	while(ptok(&p)->kind != TK_EOF) {
		bool is_typedef = false;
		bool is_extern = false;
		bool is_static = false;
		bool is_inline = false;
		CType *base_type;
		Declarator q;
		Decl *declaration;
		for(;;) {
			if(paccept(&p, "typedef")) {
				if(is_typedef)
					perr(&p, "duplicate typedef specifier");
				is_typedef = true;
				continue;
			}
			if(paccept(&p, "extern")) {
				if(is_extern)
					perr(&p, "duplicate extern specifier");
				is_extern = true;
				continue;
			}
			if(paccept(&p, "static")) {
				if(is_static)
					perr(&p, "duplicate static specifier");
				is_static = true;
				continue;
			}
			if(paccept(&p, "inline")) {
				if(is_inline)
					perr(&p, "duplicate inline specifier");
				is_inline = true;
				continue;
			}
			break;
		}
		if(is_extern && is_static)
			perr(&p, "declaration cannot be both extern and static");
		if(is_typedef && (is_extern || is_static || is_inline))
			perr(&p, "typedef cannot be combined with extern, static, or inline");
		base_type = parse_type(&p);
		if(paccept(&p, ";")) {
			if(is_typedef || is_extern || is_static || is_inline)
				perr(&p, "declaration specifier requires a declarator");
			continue;
		}
		q = parse_declarator(&p, base_type, false);
		if(is_typedef) {
			if(q.function)
				perr(&p, "function typedefs are not supported yet");
			pexpect(&p, ";");
			add_alias(&p, q.name, q.type);
			continue;
		}
		if(is_inline && !q.function)
			perr(&p, "inline can only be used on a function");
		declaration = new_decl();
		declaration->name = q.name;
		declaration->type = q.type;
		declaration->params = q.params;
		declaration->nparams = q.nparams;
		declaration->capparams = q.capparams;
		declaration->variadic = q.variadic;
		declaration->is_extern = is_extern;
		declaration->is_static = is_static;
		declaration->is_inline = is_inline;
		if(q.function) {
			if(paccept(&p, ";"))
				declaration->prototype = true;
			else {
				if(is_extern)
					perr(&p, "extern function cannot have a body");
				declaration->body = parse_block(&p);
			}
		} else {
			if(paccept(&p, "=")) {
				if(is_extern)
					perr(&p, "extern object cannot have an initializer");
				declaration->init = parse_initializer(&p);
			}
			infer_array_bound(&p, declaration->type, declaration->init);
			pexpect(&p, ";");
		}
		ARR_GROW(prog->a, prog->n, prog->cap, Decl *);
		prog->a[prog->n++] = declaration;
	}
}
