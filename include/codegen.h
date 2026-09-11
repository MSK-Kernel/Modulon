#ifndef CODEGEN_H
#define CODEGEN_H

#include "parser.h"

long align_up(long value, long array);
char *generate(Program *pointer);

#endif
