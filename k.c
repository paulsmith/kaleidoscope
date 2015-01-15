#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// Prototypes

typedef struct str_node str_node;
typedef struct expr_ast expr_ast;
static expr_ast *parse_expr();

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

/**
 * AST
 */

enum expr {
    number_expr,
    variable_expr,
    binary_expr,
    call_expr
};

struct bin_op {
    char op;
    expr_ast *lhs;
    expr_ast *rhs;
};

struct fn_call {
    char *callee;
    struct expr_ast_node *args;
};

struct expr_ast {
    enum expr expr;
    union {
        double num_val;
        char *var_name;
        struct bin_op bin_op;
        struct fn_call fn_call;
    };
};

typedef struct {
    char *name;
    str_node *args;
} prototype_ast;

typedef struct {
    prototype_ast *proto;
    expr_ast *body;
} function_ast;

/**
 * Parser
 */

static int cur_tok;
static int get_next_token() {
    return cur_tok = gettok();
}

expr_ast *error(const char *str) {
    fprintf(stderr, "error: %s\n", str);
    return NULL;
}

prototype_ast *error_p(const char *str) {
    error(str);
    return NULL;
}

function_ast *error_f(const char *str) {
    error(str);
    return NULL;
}

// Linked lists

typedef struct expr_ast_node {
    expr_ast *e;
    struct expr_ast_node *next;
} expr_ast_node;

void expr_ast_node_append(expr_ast_node **head, expr_ast *e) {
    expr_ast_node *cur_node = *head;
    expr_ast_node *new_node;

    new_node = malloc(sizeof(expr_ast_node));
    new_node->e = e;
    new_node->next = NULL;

    if (cur_node == NULL) {
        *head = new_node;
    } else {
        while (cur_node->next != NULL) {
            cur_node = cur_node->next;
        }
        cur_node->next = new_node;
    }
}

struct str_node {
    char *s;
    struct str_node *next;
};

void str_node_append(str_node **head, char *s) {
    str_node *cur_node = *head;
    str_node *new_node;

    new_node = malloc(sizeof(str_node));
    new_node->s = strdup(s);
    new_node->next = NULL;

    if (cur_node == NULL) {
        *head = new_node;
    } else {
        for (; cur_node->next != NULL; cur_node = cur_node->next)
            ;
        cur_node->next = new_node;
    }
}

// Productions

// numberexpr ::= number
static expr_ast *parse_number_expr() {
    expr_ast *res = malloc(sizeof(expr_ast));
    res->expr = number_expr;
    res->num_val = num_val;
    get_next_token();
    return res;
}

// parenexpr ::= '(' expression ')'
static expr_ast *parse_paren_expr() {
    get_next_token(); // consume '('
    expr_ast *v = parse_expr();
    if (!v) return NULL;
    if (cur_tok != ')') return error("expected ')'");
    get_next_token(); // consume ')'
    return v;
}

// identexpr
//   ::= ident
//   ::= ident '(' expression* ')'
static expr_ast *parse_ident_expr() {
    char *id_name = ident_str;
    get_next_token(); // consume ident
    if (cur_tok != '(') { // simple variable ref
        expr_ast *v = malloc(sizeof(expr_ast));
        v->expr = variable_expr;
        v->var_name = strdup(id_name);
        return v;
    }

    // call.
    get_next_token(); // consume '('
    expr_ast_node *args = NULL;
    if (cur_tok != ')') {
        while (1) {
            expr_ast *arg = parse_expr();
            if (!arg) return NULL;
            expr_ast_node_append(&args, arg);
            if (cur_tok == ')') break;
            if (cur_tok != ',') return error("expected ')' or ',' in argument list");
            get_next_token();
        }
    }

    get_next_token(); // consume ')'

    expr_ast *res = malloc(sizeof(expr_ast));
    res->expr = call_expr;
    res->fn_call.callee = strdup(id_name);
    res->fn_call.args = args;
    return res;
}

static expr_ast *parse_primary() {
    switch (cur_tok) {
    case tok_ident:
        return parse_ident_expr();
    case tok_num:
        return parse_number_expr();
    case '(':
        return parse_paren_expr();
    default:
        return error("unknown token when expected an expression");
    }
}

// TODO: hash map
static int bin_op_precedence[255] = {
    ['<'] = 10,
    ['+'] = 20,
    ['-'] = 20,
    ['*'] = 40
};

static int get_tok_precedence() {
    if (!isascii(cur_tok)) {
        return -1;
    }

    int prec = bin_op_precedence[cur_tok];
    if (prec <= 0) {
        return -1;
    }
    return prec;
}

static expr_ast *parse_bin_op_rhs(int expr_prec, expr_ast *lhs) {
    // If this is a binop, find its precedence.
    while (1) {
        int tok_prec = get_tok_precedence();

        // If this is a binop that binds at least as tightly as the current
        // binop, consume it, otherwise we are done.
        if (tok_prec < expr_prec) {
            return lhs;
        }

        // Okay, we know this is a binop.
        int bin_op = cur_tok;
        get_next_token(); // eat binop;

        expr_ast *rhs = parse_primary();
        if (!rhs) {
            return NULL;
        }

        // If binop binds less tightly with RHS than the operator after RHS, let
        // the pending operator take RHS as its LHS.
        int next_prec = get_tok_precedence();
        if (tok_prec < next_prec) {
            rhs = parse_bin_op_rhs(tok_prec+1, rhs);
            if (rhs == NULL) {
                return NULL;
            }
        }

        // Merge LHS/RHS
        lhs->expr = binary_expr;
        lhs->bin_op.op = bin_op;
        lhs->bin_op.lhs = lhs;
        lhs->bin_op.rhs = rhs;
    }
}

// expression
//   ::= primary binoprhs
static expr_ast *parse_expr() {
    expr_ast *lhs = parse_primary();
    if (!lhs) {
        return NULL;
    }
    return parse_bin_op_rhs(0, lhs);
}

// prototype
//  ::= id '(' id* ')'
static prototype_ast *parse_prototype() {
    if (cur_tok != tok_ident) {
        return error_p("expected function name in prototype");
    }

    char *fn_name = ident_str;
    get_next_token();

    if (cur_tok != '(') {
        return error_p("expected '(' in prototype");
    }

    str_node *args = NULL;
    while (get_next_token() == tok_ident) {
        str_node_append(&args, ident_str);
    }

    if (cur_tok != ')') {
        return error_p("expected ')' in prototype");
    }

    // Success.
    get_next_token(); // consume ')'

    prototype_ast *res = malloc(sizeof(prototype_ast));
    res->name = strdup(fn_name);
    res->args = args;
    return res;
}

// definition ::= 'def' prototype expression
static function_ast *parse_definition() {
    get_next_token(); // consume def
    prototype_ast *proto = parse_prototype();
    if (proto == NULL) return NULL;
    expr_ast *e = NULL;
    if ((e = parse_expr()) != NULL) {
        function_ast *res = malloc(sizeof(function_ast));
        res->proto = proto;
        res->body = e;
        return res;
    }
    return NULL;
}

// external ::= 'extern' prototype
static prototype_ast *parse_extern() {
    get_next_token(); // consume extern
    return parse_prototype();
}

// toplevelexpr ::= expression
static function_ast *parse_top_level_expr() {
    expr_ast *e = NULL;
    if ((e = parse_expr()) != NULL) {
        // Make an anonymous proto.
        prototype_ast *proto = malloc(sizeof(prototype_ast));
        proto->name = "";
        proto->args = NULL;
        function_ast *res = malloc(sizeof(function_ast));
        res->proto = proto;
        res->body = e;
        return res;
    }
    return NULL;
}

/**
 * Top-level parsing
 */

static void handle_definition() {
    if (parse_definition()) {
        fprintf(stderr, "parsed a function definition.\n");
    } else {
        // skip token for error recovery.
        get_next_token();
    }
}

static void handle_extern() {
    if (parse_extern()) {
        fprintf(stderr, "parsed an extern.\n");
    } else {
        // skip token for error recovery.
        get_next_token();
    }
}

static void handle_top_level_expr() {
    if (parse_top_level_expr()) {
        fprintf(stderr, "parsed a top-level expr.\n");
    } else {
        // skip token for error recovery.
        get_next_token();
    }
}

// top ::= definition | external | expression | ';'
static void main_loop() {
    while (1) {
        fprintf(stderr, "ready> ");
        switch (cur_tok) {
        case tok_eof: return;
        case ';': get_next_token(); break;
        case tok_def: handle_definition(); break;
        case tok_extern: handle_extern(); break;
        default: handle_top_level_expr();
        }
    }
}

/**
 * Test runners
 */

void test_lexer(void) {
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

/**
 * Main
 */

int main(void) {
    // Prime the first token.
    fprintf(stderr, "ready> ");
    get_next_token();

    main_loop();
}
