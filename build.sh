#!/bin/sh

START=$(date +%s%N)
CC="cc -std=c11 -Wall -Wextra -Wno-unused-parameter -Iinclude -c"
log()
{
	NOW=$(date +%s%N)
	ELAPSED=$((NOW - START))
	SECONDS=$((ELAPSED / 1000000000))
	MILLISECONDS=$((ELAPSED / 1000000 % 1000))
	printf "[%d:%03d] %s\n" "$SECONDS" "$MILLISECONDS" "$1"
}

log "Modulon build start"
log "Init bin/"
mkdir -p bin
log "CC=$CC"
log "Compiling source code..."
log "CC src/assembler.c"
$CC src/assembler.c
log "CC src/ast.c"
$CC src/ast.c
log "CC src/codegen.c"
$CC src/codegen.c
log "CC src/common.c"
$CC src/common.c
log "CC src/i386.c"
$CC src/i386.c
log "CC src/jit.c"
$CC src/jit.c
log "CC src/lexer.c"
$CC src/lexer.c
log "CC src/main.c"
$CC src/main.c
log "CC src/parser.c"
$CC src/parser.c
log "CC src/type.c"
$CC src/type.c
log "Linking object files..."
log "cc *.o -o bin/mlonc"
cc *.o -o bin/mlonc
log "Removing object files..."
rm *.o
log "Modulon build end"