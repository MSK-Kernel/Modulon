//Modulon JIT include

#ifndef JIT_H
#define JIT_H

#include "parser.h"

typedef struct JitModule JitModule;
typedef void *(*JitResolver)(const char *name, void *context);
//Compile the program and return a live module, or die.
JitModule *jit_compile(Program *program, JitResolver resolver, void *resolver_context);
//Address of an internal symbol (function, global, or string label), or NULL.
void *jit_symbol(JitModule *module, const char *name);
//Call an entry function.
int jit_run(JitModule *module, const char *entry, int argc, char **argv);
//Release a module and all memory it owns.
void jit_free(JitModule *module);
#endif
