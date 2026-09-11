#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include "common.h"

typedef enum
{
	ASEC_TEXT,
	ASEC_RODATA,
	ASEC_BSS
} ASection;

typedef struct
{
	char *name;
	ASection section;
	uint64_t offset;
} ALabel;

typedef enum
{
	FIX_REL32_SYMBOL,
	FIX_RIP32_SYMBOL,
	FIX_RIP32_OBJECT,
	FIX_RIP32_GOT
} FixKind;

typedef struct
{
	FixKind kind;
	uint64_t offset;
	char *name;
	long aux;
} Fixup;

typedef struct
{
	char *name;
	bool object;
	uint32_t symbol_index;
	uint32_t got_index;
	uint32_t plt_reloc_index;
	uint64_t plt_offset;
	uint32_t string_offset;
} Import;

typedef struct
{
	Buf text;
	Buf rodata;
	uint64_t bss_size;
	ASection section;
	ALabel *labels;
	size_t nlabels;
	size_t caplabels;
	Fixup *fixups;
	size_t nfixups;
	size_t capfixups;
	Import *imports;
	size_t nimports;
	size_t capimports;
	uint64_t plt0_offset;
} AsmImage;

void put_u8(Buf *buffer, uint8_t value);
void internal_assemble(const char *assembly, AsmImage *array);
void write_independent_elf(const char *path, AsmImage *array, char **libraries, size_t library_count);

#endif
