#ifndef I386_H
#define I386_H

#include "parser.h"
#include "codegen.h"
#include "assembler.h"

void write_i386_relocatable(const char *path, Program *program);

#endif
