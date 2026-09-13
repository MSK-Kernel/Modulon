#ifndef COMMON_H
#define COMMON_H

#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <elf.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define VERSION "Modulon 1.1-beta5"
#define ARR_GROW(a, n, cap, type)                                  \
	do                                                         \
	{                                                          \
		if((n) >= (cap))                                   \
		{                                                  \
			(cap) = (cap) ? (cap) * 2 : 16;            \
			(a) = xrealloc((a), (cap) * sizeof(type)); \
		}                                                  \
	} while(0)

typedef struct
{
	char *s;
	size_t n;
	size_t cap;
} Buf;

void fatal(const char *fmt, ...);
void *xmalloc(size_t count);
void *xcalloc(size_t argument_count, size_t element_size_1);
void *xrealloc(void *pointer, size_t count);
char *xstrdup(const char *string);
char *xstrndup2(const char *string, size_t length);
void bneed(Buf *buffer, size_t extra);
void bputn(Buf *buffer, const char *string, size_t count);
void bputs(Buf *buffer, const char *string);
void bprintf(Buf *buffer, const char *fmt, ...);
char *read_file(const char *path);
void write_file(const char *path, const char *string, size_t count);
char *preprocess_source(const char *text);

#endif
