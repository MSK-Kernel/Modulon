#include "common.h"

//init functions

void fatal(const char *fmt, ...)
{
	va_list argument_list;
	fprintf(stderr, "\033[37;41mERROR:\033[0m Compilation failed: ");
	va_start(argument_list, fmt);
	vfprintf(stderr, fmt, argument_list);
	va_end(argument_list);
	fputc('\n', stderr);
	exit(0);
}
//Compiler memory functions.

void *xmalloc(size_t count)
{
	void *position = malloc(count ? count : 1);
	if(!position)
		fatal("out of memory");
	return position;
}

void *xcalloc(size_t argument_count, size_t element_size_1)
{
	void *position = calloc(argument_count ? argument_count : 1, element_size_1 ? element_size_1 : 1);
	if(!position)
		fatal("out of memory");
	return position;
}

void *xrealloc(void *pointer, size_t count)
{
	pointer = realloc(pointer, count ? count : 1);
	if(!pointer)
		fatal("out of memory");
	return pointer;
}

char *xstrdup(const char *string)
{
	size_t length = strlen(string) + 1;
	char *position = xmalloc(length);
	memcpy(position, string, length);
	return position;
}

char *xstrndup2(const char *string, size_t length)
{
	char *position = xmalloc(length + 1);
	memcpy(position, string, length);
	position[length] = 0;
	return position;
}

void bneed(Buf *buffer, size_t extra)
{
	size_t need = buffer->n + extra + 1;
	if(need <= buffer->cap)
		return;
	if(!buffer->cap)
		buffer->cap = 4096;
	while(buffer->cap < need)
		buffer->cap *= 2;
	buffer->s = xrealloc(buffer->s, buffer->cap);
}

void bputn(Buf *buffer, const char *string, size_t count)
{
	bneed(buffer, count);
	memcpy(buffer->s + buffer->n, string, count);
	buffer->n += count;
	buffer->s[buffer->n] = 0;
}

void bputs(Buf *buffer, const char *string)
{
	bputn(buffer, string, strlen(string));
}

void bprintf(Buf *buffer, const char *fmt, ...)
{
	va_list argument_list, copied_argument_list;
	int count;
	va_start(argument_list, fmt);
	va_copy(copied_argument_list, argument_list);
	count = vsnprintf(NULL, 0, fmt, copied_argument_list);
	va_end(copied_argument_list);
	if(count < 0)
		fatal("formatting failed");
	bneed(buffer, (size_t)count);
	vsnprintf(buffer->s + buffer->n, buffer->cap - buffer->n, fmt, argument_list);
	va_end(argument_list);
	buffer->n += (size_t)count;
}

char *read_file(const char *path)
{
	FILE *file = fopen(path, "rb");
	Buf buffer = {0};
	char tmp[8192];
	size_t count;
	if(!file)
		fatal("cannot open %s: %s", path, strerror(errno));
	while((count = fread(tmp, 1, sizeof(tmp), file)) != 0)
		bputn(&buffer, tmp, count);
	if(ferror(file))
		fatal("cannot read %s", path);
	fclose(file);
	if(!buffer.s)
		buffer.s = xstrdup("");
	return buffer.s;
}

void write_file(const char *path, const char *string, size_t count)
{
	FILE *file = fopen(path, "wb");
	if(!file)
		fatal("cannot create %s: %s", path, strerror(errno));
	if(count && fwrite(string, 1, count, file) != count)
		fatal("cannot write %s", path);
	if(fclose(file))
		fatal("cannot close %s", path);
}
typedef struct
{
	char *name;
	char *value;
} Macro;

static Macro *find_macro(Macro *macros, size_t nmacros, const char *name, size_t length)
{
	size_t index;
	for(index = 0; index < nmacros; index++)
		if(strlen(macros[index].name) == length &&
		   !memcmp(macros[index].name, name, length))
			return &macros[index];
	return NULL;
}

char *preprocess_source(const char *text)
{
	Macro *macros = NULL;
	size_t nmacros = 0, capmacros = 0;
	Buf body = {0};
	Buf out = {0};
	const char *position = text;
	size_t index, count;
	enum
	{
		PP_NORMAL,
		PP_STRING,
		PP_CHAR,
		PP_LINE_COMMENT,
		PP_BLOCK_COMMENT
	} state = PP_NORMAL;
	bool escaped = false;
	while(*position) {
		const char *line = position;
		const char *end = strchr(position, '\n');
		const char *cursor;
		if(!end)
			end = position + strlen(position);
		cursor = line;
		while(cursor < end && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r'))
			cursor++;
		if(cursor < end && *cursor == '#') {
			cursor++;
			while(cursor < end && isspace((unsigned char)*cursor))
				cursor++;
			if(end - cursor >= 6 && !memcmp(cursor, "define", 6) &&
			   (cursor + 6 == end || isspace((unsigned char)cursor[6])))
			{
				const char *name;
				const char *value;
				const char *value_end;
				Macro *old;
				cursor += 6;
				while(cursor < end && isspace((unsigned char)*cursor))
					cursor++;
				name = cursor;
				if(cursor < end && (isalpha((unsigned char)*cursor) || *cursor == '_')) {
					cursor++;
					while(cursor < end && (isalnum((unsigned char)*cursor) || *cursor == '_'))
						cursor++;
					if(cursor < end && *cursor != '(') {
						value = cursor;
						while(value < end && isspace((unsigned char)*value))
							value++;
						value_end = end;
						while(value_end > value &&
						      isspace((unsigned char)value_end[-1]))
							value_end--;
						old = find_macro(macros, nmacros, name, (size_t)(cursor - name));
						if(!old) {
							ARR_GROW(macros, nmacros, capmacros, Macro);
							old = &macros[nmacros++];
							old->name = xstrndup2(name, (size_t)(cursor - name));
						} else {
							free(old->value);
						}
						old->value = xstrndup2(value,
								       (size_t)(value_end - value));
					}
				}
			}
			bputn(&body, "\n", 1);
		} else {
			bputn(&body, line, (size_t)(end - line));
			if(*end == '\n')
				bputn(&body, "\n", 1);
		}
		position = *end ? end + 1 : end;
	}
	if(!body.s)
		body.s = xstrdup("");
	count = body.n;
	for(index = 0; index < count;) {
		char character = body.s[index];
		char next = index + 1 < count ? body.s[index + 1] : 0;
		if(state == PP_NORMAL) {
			if(character == '/' && next == '/') {
				bputn(&out, "//", 2);
				index += 2;
				state = PP_LINE_COMMENT;
				continue;
			}
			if(character == '/' && next == '*') {
				bputn(&out, "/*", 2);
				index += 2;
				state = PP_BLOCK_COMMENT;
				continue;
			}
			if(character == '"' || character == '\'') {
				bputn(&out, &character, 1);
				index++;
				state = character == '"' ? PP_STRING : PP_CHAR;
				escaped = false;
				continue;
			}
			if(isalpha((unsigned char)character) || character == '_') {
				size_t start = index;
				Macro *macro;
				index++;
				while(index < count && (isalnum((unsigned char)body.s[index]) ||
						body.s[index] == '_'))
					index++;
				macro = find_macro(macros, nmacros, body.s + start, index - start);
				if(macro) {
					bputn(&out, "(", 1);
					bputs(&out, macro->value);
					bputn(&out, ")", 1);
				} else {
					bputn(&out, body.s + start, index - start);
				}
				continue;
			}
			bputn(&out, &character, 1);
			index++;
			continue;
		}
		bputn(&out, &character, 1);
		index++;
		if(state == PP_LINE_COMMENT) {
			if(character == '\n')
				state = PP_NORMAL;
		} else if(state == PP_BLOCK_COMMENT)
		{
			if(character == '*' && next == '/') {
				bputn(&out, "/", 1);
				index++;
				state = PP_NORMAL;
			}
		} else if(escaped)
		{
			escaped = false;
		} else if(character == '\\')
		{
			escaped = true;
		} else if((state == PP_STRING && character == '"') ||
			  (state == PP_CHAR && character == '\''))
		{
			state = PP_NORMAL;
		}
	}
	if(!out.s)
		out.s = xstrdup("");
	return out.s;
}
