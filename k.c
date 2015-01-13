#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

/**
 * Lexer
 */

typedef enum {
    tok_eof = -1,
    tok_def = -2,
    tok_extern = -3,
    tok_ident = -4,
    tok_num = -5,
} token;

static char ident_str[64];
static double num_val;

static int gettok() {
    static int last_char = ' ';
    char *s;

    while (isspace((last_char)))
        last_char = getchar();

    if (isalpha(last_char)) {
        s = ident_str;
        *s++ = last_char;
        while (isalnum((last_char = getchar()))) {
            *s++ = last_char;
        }
        *s = '\0';

        if (strncmp(ident_str, "def", s-ident_str) == 0) return tok_def;
        if (strncmp(ident_str, "extern", s-ident_str) == 0) return tok_extern;

        return tok_ident;
    }

    if (isdigit(last_char) || last_char == '.') {
        char num_str[64];
        char *t;
        t = num_str;
        do {
            *t++ = last_char;
            last_char = getchar();
        } while (isdigit(last_char) || last_char == '.');
        *t = '\0';
        num_val = atof(num_str);
        return tok_num;
    }

    if (last_char == '#') {
        do {
            last_char = getchar();
        } while (last_char != EOF && last_char != '\n' && last_char != '\r');

        if (last_char != EOF) {
            return gettok();
        }
    }

    if (last_char == EOF) {
        return tok_eof;
    }

    int this_char = last_char;
    last_char = getchar();
    return this_char;
}

int main(void) {
    int tok;
    while ((tok = gettok()) != tok_eof) {
        switch (tok) {
        case tok_def:
            printf("DEF\n");
            break;
        case tok_extern:
            printf("EXTERN\n");
            break;
        case tok_ident:
            printf("IDENT(%s)\n", ident_str);
            break;
        case tok_num:
            printf("NUM(%f)\n", num_val);
            break;
        default:
            printf("LITERAL '%c'\n", tok);
        }
    }
}
