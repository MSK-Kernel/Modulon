#include "type.h"

CType T_VOID = {.kind = TY_VOID, .name = "void"};
CType T_CHAR = {.kind = TY_CHAR, .name = "char"};
CType T_SHORT = {.kind = TY_SHORT, .name = "short"};
CType T_INT = {.kind = TY_INT, .name = "int"};
CType T_U8 = {.kind = TY_U8, .name = "unsigned char"};
CType T_U16 = {.kind = TY_U16, .name = "unsigned short"};
CType T_U32 = {.kind = TY_U32, .name = "unsigned int"};
CType T_U64 = {.kind = TY_U64, .name = "unsigned long long"};
CType T_DOUBLE = {.kind = TY_DOUBLE, .name = "double"};
CType T_XEVENT = {.kind = TY_XEVENT, .name = "XEvent"};
CType T_VALIST = {.kind = TY_VALIST, .name = "va_list"};
CType T_DISPLAY = {.kind = TY_OPAQUE, .name = "Display"};
CType T_FILE = {.kind = TY_OPAQUE, .name = "FILE"};

CType *new_type(TypeKind third_index, CType *base, long count, const char *name)
{
	CType *type = xcalloc(1, sizeof(*type));
	type->kind = third_index;
	type->base = base;
	type->count = count;
	type->name = name;
	return type;
}

CType *ptr_to(CType *base)
{
	return new_type(TY_PTR, base, 0, NULL);
}

CType *array_of(CType *base, long count)
{
	return new_type(TY_ARRAY, base, count, NULL);
}

CType *vla_of(CType *base, const char *bound)
{
	return new_type(TY_ARRAY, base, -1, xstrdup(bound));
}

bool is_vla(CType *type)
{
	return type && type->kind == TY_ARRAY && type->count < 0;
}

long type_size(CType *type)
{
	switch(type->kind) {
	case TY_VOID:
		return 0;
	case TY_CHAR:
	case TY_U8:
		return 1;
	case TY_SHORT:
	case TY_U16:
		return 2;
	case TY_INT:
	case TY_U32:
		return 4;
	case TY_U64:
	case TY_DOUBLE:
	case TY_PTR:
	case TY_OPAQUE:
		return 8;
	case TY_STRUCT:
		return type->size;
	case TY_ARRAY:
		if(is_vla(type))
			return 8;
		return type_size(type->base) * type->count;
	case TY_XEVENT:
		return 192;
	case TY_VALIST:
		return 24;
	}
	fatal("internal: unknown type");
	return 0;
}

long type_align(CType *type)
{
	if(type->kind == TY_CHAR || type->kind == TY_U8)
		return 1;
	if(type->kind == TY_SHORT || type->kind == TY_U16)
		return 2;
	if(type->kind == TY_INT || type->kind == TY_U32)
		return 4;
	if(type->kind == TY_ARRAY)
		return type_align(type->base);
	if(type->kind == TY_STRUCT)
		return type->align ? type->align : 1;
	return 8;
}
