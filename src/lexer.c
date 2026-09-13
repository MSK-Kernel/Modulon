#include "lexer.h"


static void tok_push(Tokens *token_stream, TokKind third_index, const char *string, size_t count, int line, int col)
{
	ARR_GROW(token_stream->a, token_stream->n, token_stream->cap, Token);
	token_stream->a[token_stream->n].kind = third_index;
	token_stream->a[token_stream->n].v = xstrndup2(string, count);
	token_stream->a[token_stream->n].line = line;
	token_stream->a[token_stream->n].col = col;
	token_stream->n++;
}

static bool starts(const char *string, size_t index, const char *x_value)
{
	return !strncmp(string + index, x_value, strlen(x_value));
}

Tokens lex_source(const char *text, const char *file)
{
	Tokens ts = {0};
	size_t index = 0, count = strlen(text);
	int line = 1, col = 1;
	bool bol = true;
	while(index < count) {
		char character = text[index];
		if(bol) {
			size_t inner_index = index;
			int ccol = col;
			while(inner_index < count && (text[inner_index] == ' ' || text[inner_index] == '\t' || text[inner_index] == '\r')) {
				inner_index++;
				ccol++;
			}
			if(inner_index < count && text[inner_index] == '#') {
				bool cont;
				do {
					cont = false;
					while(index < count && text[index] != '\n') {
						if(text[index] == '\\')
							cont = true;
						else if(text[index] != '\r' && !isspace((unsigned char)text[index]))
							cont = false;
						index++;
						col++;
					}
					if(index < count && text[index] == '\n') {
						index++;
						line++;
						col = 1;
						bol = true;
					}
				} while(cont && index < count);
				continue;
			}
		}
		if(isspace((unsigned char)character)) {
			if(character == '\n') {
				line++;
				col = 1;
				bol = true;
			} else
				col++;
			index++;
			continue;
		}
		bol = false;
		if(index + 1 < count && text[index] == '/' && text[index + 1] == '/') {
			index += 2;
			col += 2;
			while(index < count && text[index] != '\n') {
				index++;
				col++;
			}
			continue;
		}
		if(index + 1 < count && text[index] == '/' && text[index + 1] == '*') {
			index += 2;
			col += 2;
			while(index + 1 < count && !(text[index] == '*' && text[index + 1] == '/')) {
				if(text[index] == '\n') {
					line++;
					col = 1;
					bol = true;
					index++;
				} else {
					index++;
					col++;
				}
			}
			if(index + 1 >= count)
				fatal("%s:%d:%d: unterminated comment", file, line, col);
			index += 2;
			col += 2;
			continue;
		}
		{
			int source_line = line, scan_code = col;
			size_t start = index;
			if(isalpha((unsigned char)character) || character == '_') {
				index++;
				col++;
				while(index < count && (isalnum((unsigned char)text[index]) || text[index] == '_')) {
					index++;
					col++;
				}
				tok_push(&ts, TK_ID, text + start, index - start, source_line, scan_code);
				continue;
			}
			if(isdigit((unsigned char)character)) {
				bool floating = false;
				index++;
				col++;
				if(character == '0' && index < count && (text[index] == 'x' || text[index] == 'X')) {
					index++;
					col++;
					while(index < count && isxdigit((unsigned char)text[index])) {
						index++;
						col++;
					}
				} else {
					while(index < count && isdigit((unsigned char)text[index])) {
						index++;
						col++;
					}
					if(index < count && text[index] == '.' && !(index + 1 < count && text[index + 1] == '.')) {
						floating = true;
						index++;
						col++;
						while(index < count && isdigit((unsigned char)text[index])) {
							index++;
							col++;
						}
					}
					if(index < count && (text[index] == 'e' || text[index] == 'E')) {
						floating = true;
						index++;
						col++;
						if(index < count && (text[index] == '+' || text[index] == '-')) {
							index++;
							col++;
						}
						while(index < count && isdigit((unsigned char)text[index])) {
							index++;
							col++;
						}
					}
				}
				if(floating) {
					if(index < count && (text[index] == 'f' || text[index] == 'F' || text[index] == 'l' || text[index] == 'L')) {
						index++;
						col++;
					}
				} else {
					while(index < count && strchr("uUlL", text[index])) {
						index++;
						col++;
					}
				}
				tok_push(&ts, TK_NUM, text + start, index - start, source_line, scan_code);
				continue;
			}
			if(character == '"' || character == '\'') {
				char cursor = character;
				bool esc = false;
				index++;
				col++;
				while(index < count) {
					char value = text[index++];
					col++;
					if(value == '\n') {
						line++;
						col = 1;
					}
					if(esc)
						esc = false;
					else if(value == '\\')
						esc = true;
					else if(value == cursor)
						break;
				}
				if(text[index - 1] != cursor)
					fatal("%s:%d:%d: unterminated literal", file, source_line, scan_code);
				tok_push(&ts, cursor == '"' ? TK_STR : TK_CHAR, text + start, index - start, source_line, scan_code);
				continue;
			}
			{
				static const char *ops[] = {
					"<<=", ">>=", "...", "..", "==", "!=", "<=", ">=", "&&", "||", "++", "--", "->", "<<", ">>", "+=", "-=", "*=", "/=", "%=", "&=", "|=", "^=", NULL};
				int third_index;
				for(third_index = 0; ops[third_index]; third_index++) {
					size_t token_length = strlen(ops[third_index]);
					if(index + token_length <= count && starts(text, index, ops[third_index])) {
						tok_push(&ts, TK_OP, text + index, token_length, source_line, scan_code);
						index += token_length;
						col += (int)token_length;
						goto token_done;
					}
				}
			}
			if(strchr("{}[]();,.*&+-/%!~<>=|^?:", character)) {
				tok_push(&ts, TK_OP, text + index, 1, source_line, scan_code);
				index++;
				col++;
				continue;
			}
			fatal("%s:%d:%d: unexpected character '%c'", file, line, col, character);
		}
	token_done:;
	}
	tok_push(&ts, TK_EOF, "<eof>", 5, line, col);
	ts.file = file;
	return ts;
}
