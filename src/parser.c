#include "parser.h"

#include <errno.h>
#include <limits.h>
#include <ctype.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

static void parser_skip_ignoreable(struct Parser *parser);
static bool parse_token(struct Parser *parser);

static bool parser_peek_token(struct Parser *parser);
static bool parser_consume_token(struct Parser *parser);
static bool parser_append_node(struct Parser *parser, const struct AstNode *node);
static size_t parser_get_arg_index(struct Parser *parser, const char *name);

static bool parse_expr(struct Parser *parser);
static bool parse_add_sub(struct Parser *parser, struct AstNode *node);
static bool parse_mul_div(struct Parser *parser, struct AstNode *node);
static bool parse_signed(struct Parser *parser, struct AstNode *node);
static bool parse_atom(struct Parser *parser, struct AstNode *node);
static bool parse_paren(struct Parser *parser, struct AstNode *node);

size_t parser_get_arg_index(struct Parser *parser, const char *name) {
    size_t argindex = 0;

    for (; argindex < parser->argc; ++ argindex) {
        if (strcmp(parser->args[argindex], name) == 0) {
            break;
        }
    }

    return argindex;
}

void parser_skip_ignoreable(struct Parser *parser) {
    while (parser->index < parser->code_size) {
        char sym = parser->code[parser->index];

        if (sym == '#') { // comment
            ++ parser->index;

            while (parser->index < parser->code_size && parser->code[parser->index] != '\n') {
                ++ parser->index;
            }
        } else if (!isspace(sym)) {
            break;
        } else {
            ++ parser->index;
        }
    }
}

struct Location get_location(const char *code, size_t size, size_t index) {
    const char *ptr = code;
    const char *next_ptr = NULL;
    const char *end_ptr = code + (index > size ? size : index);
    struct Location loc = { .lineno = 1, .column = 1 };

    while ((next_ptr = memchr(ptr, '\n', (size_t)(end_ptr - ptr)))) {
        ++ loc.lineno;
        ptr = next_ptr + 1;
    }

    loc.column = (size_t)(end_ptr - ptr) + 1;

    return loc;
}

size_t get_line_start(const char *code, size_t size, size_t index) {
    const char *startptr = index < size ?
        memrchr(code, '\n', index + 1) :
        memrchr(code, '\n', size);

    return startptr == NULL ? 0 : startptr - code + 1;
}

size_t get_line_end(const char *code, size_t size, size_t index) {
    if (index >= size) {
        return size;
    }
    const char *endptr = memchr(code + index, '\n', size - index);
    return endptr == NULL ? size : (size_t)(endptr - code);
}

static size_t get_number_length(size_t number) {
    if (number == 0) {
        return 1;
    }
    size_t length = 0;
    while (number > 0) {
        number /= 10;
        ++ length;
    }
    return length;
}

struct Range get_line_range(const char *code, size_t size, size_t index) {
    const char *startptr;
    const char *endptr;

    if (index < size) {
        startptr = memrchr(code, '\n', index + 1);
        endptr   = memchr(code + index, '\n', size - index);
    } else {
        startptr = memrchr(code, '\n', size);
        endptr   = code + size;
    }

    return (struct Range) {
        .start_index = startptr == NULL ?    0 : (size_t)(startptr - code + 1),
        .end_index   = endptr   == NULL ? size : (size_t)(endptr - code),
    };
}

bool parse_token(struct Parser *parser) {
    parser_skip_ignoreable(parser);

    if (parser->index >= parser->code_size) {
        parser->state = PARSER_TOKEN_READY;
        parser->token.type = TOK_EOF;
        parser->token.start_index = parser->code_size;
        parser->token.end_index   = parser->code_size;
        return true;
    }

    char sym = parser->code[parser->index];

    switch (sym) {
        case '+':
        case '-':
        case '*':
        case '/':
        case '(':
        case ')':
        parser->token.start_index = parser->index;
        parser->token.end_index   = parser->index + 1;
            parser->state = PARSER_TOKEN_READY;
            parser->token.type = (enum TokenType) sym;
            ++ parser->index;
            return true;

        default:
            if (sym >= '0' && sym <= '9') {
                parser->token.start_index = parser->index;
                long value = 0;

                while (parser->index < parser->code_size) {
                    sym = parser->code[parser->index];

                    if (sym < '0' || sym > '9') {
                        break;
                    }

                    const int digit = sym - '0';

                    if (value > (LONG_MAX - digit) / 10) {
                        parser->state = PARSER_ERROR;
                        parser->error = ERROR_VALUE_OUT_OF_RANGE;
                        parser->error_info.code.start_index = parser->token.start_index;
                        parser->error_info.code.end_index   = parser->index;
                        return false;
                    }

                    value *= 10;
                    value += digit;

                    ++ parser->index;
                }
                parser->state = PARSER_TOKEN_READY;
                parser->token.end_index = parser->index;
                parser->token.type = TOK_INT;
                parser->token.value = value;

                return true;
            } else if (IS_IDENT_HEAD(sym)) {
                parser->token.start_index = parser->index;

                while (parser->index < parser->code_size) {
                    sym = parser->code[parser->index];

                    if (!IS_IDENT_TAIL(sym)) {
                        break;
                    }

                    ++ parser->index;
                }

                parser->token.end_index = parser->index;
                const size_t namelen = parser->index - parser->token.start_index;

                buffer_clear(&parser->buffer);

                if (!buffer_append(&parser->buffer, parser->code + parser->token.start_index, namelen)) {
                    parser->state = PARSER_ERROR;
                    parser->error = ERROR_OUT_OF_MEMORY;
                    parser->error_info.code.start_index = parser->token.start_index;
                    parser->error_info.code.end_index   = parser->token.end_index;
                    return false;
                }

                if (!buffer_append_byte(&parser->buffer, 0)) {
                    parser->state = PARSER_ERROR;
                    parser->error = ERROR_OUT_OF_MEMORY;
                    parser->error_info.code.start_index = parser->token.start_index;
                    parser->error_info.code.end_index   = parser->token.end_index;
                    return false;
                }

                parser->state = PARSER_TOKEN_READY;
                parser->token.type = TOK_IDENT;
                parser->token.name = parser->buffer.data;

                return true;
            } else {
                parser->state = PARSER_ERROR;
                parser->error = ERROR_ILLEGAL_CHARACTER;
                parser->error_info.code.start_index = parser->index;
                parser->error_info.code.end_index   = parser->index + 1;

                return false;
            }
    }
}

struct Parser parse_string(const char *code, char *const *const args, size_t argc) {
    return parse_slice(code, strlen(code), args, argc);
}

struct Parser parse_slice(const char *code, size_t code_size, char *const *const args, size_t argc) {
    struct Parser parser = {
        .args = args,
        .argc = argc,
        .state = PARSER_TOKEN_PENDING,
        .error = ERROR_NONE,
        .error_info = {
            .code = {
                .start_index = 0,
                .end_index   = 0,
            }
        },
        .code = code,
        .code_size = code_size,
        .index = 0,
        .token = {
            .type = TOK_EOF,
        },
        .ast = AST_INIT,
        .buffer = BUFFER_INIT,
    };

    for (size_t arg_index = 0; arg_index < argc; ++ arg_index) {
        if (!is_identifier(args[arg_index])) {
            parser.state = PARSER_ERROR;
            parser.error = ERROR_ILLEGAL_ARG_NAME;
            parser.error_info.arg_index = arg_index;
            return parser;
        }

        for (size_t other_index = 0; other_index < arg_index; ++ other_index) {
            if (strcmp(args[arg_index], args[other_index]) == 0) {
                parser.state = PARSER_ERROR;
                parser.error = ERROR_DUPLICATED_ARG_NAME;
                parser.error_info.arg_index = arg_index;
                return parser;
            }
        }
    }

    if (!parse_expr(&parser)) {
        return parser;
    }

    if (!parser_peek_token(&parser)) {
        return parser;
    }

    if (parser.token.type != TOK_EOF) {
        parser.state = PARSER_ERROR;
        parser.error = ERROR_ILLEGAL_TOKEN;
        parser.error_info.code.start_index = parser.token.start_index;
        parser.error_info.code.end_index   = parser.token.end_index;
        return parser;
    }

    if (!parser_consume_token(&parser)) {
        return parser;
    }

    parser.state = PARSER_DONE;

    return parser;
}

bool parser_peek_token(struct Parser *parser) {
    switch (parser->state) {
    case PARSER_TOKEN_READY:
        return true;

    case PARSER_TOKEN_PENDING:
        return parse_token(parser);

    default:
        return false;
    }
}

bool parser_consume_token(struct Parser *parser) {
    switch (parser->state) {
    case PARSER_TOKEN_READY:
        parser->state = PARSER_TOKEN_PENDING;
        return true;

    case PARSER_TOKEN_PENDING:
        assert(false);
        return true;

    default:
        return false;
    }
}

bool parser_append_node(struct Parser *parser, const struct AstNode *node) {
    if (!ast_append_node(&parser->ast, node)) {
        parser->state = PARSER_ERROR;
        parser->error = ERROR_OUT_OF_MEMORY;
        parser->error_info.code.start_index = node->start_index;
        parser->error_info.code.end_index   = node->end_index;
        return false;
    }

    return true;
}

bool parse_expr(struct Parser *parser) {
    struct AstNode node;
    if (!parse_add_sub(parser, &node)) {
        return false;
    }

    return parser_append_node(parser, &node);
}

bool parse_add_sub(struct Parser *parser, struct AstNode *node) {
    if (!parse_mul_div(parser, node)) {
        return false;
    }

    for (;;) {
        if (!parser_peek_token(parser)) {
            return false;
        }

        const enum TokenType token_type = parser->token.type;
        const size_t start_index = parser->token.start_index;
        const size_t end_index   = parser->token.end_index;

        if (token_type != TOK_PLUS && token_type != TOK_MINUS) {
            break;
        }

        if (!parser_append_node(parser, node) || !parser_consume_token(parser)) {
            return false;
        }

        const size_t left_index = AST_ROOT_NODE_INDEX(&parser->ast);
        struct AstNode right;

        if (!parse_mul_div(parser, &right)) {
            return false;
        }

        if (!parser_append_node(parser, &right)) {
            return false;
        }
        const size_t right_index = AST_ROOT_NODE_INDEX(&parser->ast);

        *node = (struct AstNode) {
            .type = (enum NodeType) token_type,
            .start_index = start_index,
            .end_index   = end_index,
            .binary = {
                .left_index  = left_index,
                .right_index = right_index,
            }
        };
    }

    return true;
}

bool parse_mul_div(struct Parser *parser, struct AstNode *node) {
    if (!parse_signed(parser, node)) {
        return false;
    }

    for (;;) {
        if (!parser_peek_token(parser)) {
            return false;
        }

        const enum TokenType token_type = parser->token.type;
        const size_t start_index = parser->token.start_index;
        const size_t end_index   = parser->token.end_index;

        if (token_type != TOK_MUL && token_type != TOK_DIV) {
            break;
        }

        if (!parser_append_node(parser, node) || !parser_consume_token(parser)) {
            return false;
        }

        const size_t left_index = AST_ROOT_NODE_INDEX(&parser->ast);
        struct AstNode right;

        if (!parse_signed(parser, &right)) {
            return false;
        }

        if (token_type == TOK_DIV && right.type == NODE_INT && right.value == 0) {
            parser->state = PARSER_ERROR;
            parser->error = ERROR_DIV_BY_ZERO;
            parser->error_info.code.start_index = right.start_index;
            parser->error_info.code.end_index   = right.end_index;
            return false;
        }

        if (!parser_append_node(parser, &right)) {
            return false;
        }
        const size_t right_index = AST_ROOT_NODE_INDEX(&parser->ast);

        *node = (struct AstNode) {
            .type = (enum NodeType) token_type,
            .start_index = start_index,
            .end_index   = end_index,
            .binary = {
                .left_index  = left_index,
                .right_index = right_index,
            }
        };
    }

    return true;
}

bool parse_signed(struct Parser *parser, struct AstNode *node) {
    bool inverse = false;

    if (!parser_peek_token(parser)) {
        return false;
    }

    const size_t start_index = parser->token.start_index;

    for (;;) {
        // fold series of signs
        if (!parser_peek_token(parser)) {
            return false;
        }

        const enum TokenType token_type = parser->token.type;

        if (token_type == TOK_MINUS) {
            inverse = !inverse;
        } else if (token_type != TOK_PLUS) {
            break;
        }

        if (!parser_consume_token(parser)) {
            return false;
        }
    }

    if (inverse) {
        struct AstNode child;

        if (!parse_atom(parser, &child)) {
            return false;
        }

        if (child.type == NODE_INT) {
            // apply sign to integer
            if (child.value == LONG_MAX) {
                parser->state = PARSER_ERROR;
                parser->error = ERROR_VALUE_OUT_OF_RANGE;
                parser->error_info.code.start_index = start_index;
                parser->error_info.code.end_index   = child.end_index;
                return false;
            }

            *node = (struct AstNode) {
                .type        = NODE_INT,
                .start_index = start_index,
                .end_index   = child.end_index,
                .value       = -child.value,
            };
            return true;
        }

        if (!parser_append_node(parser, &child)) {
            return false;
        }

        *node = (struct AstNode) {
            .type        = NODE_INV,
            .start_index = start_index,
            .end_index   = child.end_index,
            .child_index = AST_ROOT_NODE_INDEX(&parser->ast),
        };

        return true;
    } else {
        if (!parse_atom(parser, node)) {
            return false;
        }
        node->start_index = start_index;
        return true;
    }
}

bool parse_atom(struct Parser *parser, struct AstNode *node) {
    if (!parser_peek_token(parser)) {
        return false;
    }

    const enum TokenType token_type = parser->token.type;

    switch (token_type) {
        case TOK_IDENT:
            if (!parser_consume_token(parser)) {
                return false;
            }
            const size_t arg_index = parser_get_arg_index(parser, parser->token.name);

            if (arg_index == parser->argc) {
                parser->state = PARSER_ERROR;
                parser->error = ERROR_UNDEFINED_VARIABLE;
                parser->error_info.code.start_index = parser->token.start_index;
                parser->error_info.code.end_index   = parser->token.end_index;
                return false;
            }

            *node = (struct AstNode) {
                .type        = NODE_VAR,
                .start_index = parser->token.start_index,
                .end_index   = parser->token.end_index,
                .arg_index   = arg_index,
            };
            return true;

        case TOK_INT:
            if (!parser_consume_token(parser)) {
                return false;
            }
            *node = (struct AstNode) {
                .type        = NODE_INT,
                .start_index = parser->token.start_index,
                .end_index   = parser->token.end_index,
                .value       = parser->token.value,
            };
            return true;

        case TOK_PAREN_OPEN:
            return parse_paren(parser, node);

        default:
            parser->state = PARSER_ERROR;
            parser->error = ERROR_ILLEGAL_TOKEN;
            parser->error_info.code.start_index = parser->token.start_index;
            parser->error_info.code.end_index   = parser->token.end_index;
            return false;
    }
}

bool parse_paren(struct Parser *parser, struct AstNode *node) {
    if (!parser_peek_token(parser)) {
        return false;
    }

    if (parser->token.type != TOK_PAREN_OPEN) {
        parser->state = PARSER_ERROR;
        parser->error = ERROR_ILLEGAL_TOKEN;
        parser->error_info.code.start_index = parser->token.start_index;
        parser->error_info.code.end_index   = parser->token.end_index;
        return false;
    }

    const size_t start_index = parser->token.start_index;

    if (!parser_consume_token(parser)) {
        return false;
    }

    if (!parse_add_sub(parser, node)) {
        return false;
    }

    if (parser->token.type != TOK_PAREN_CLOSE) {
        parser->state = PARSER_ERROR;
        parser->error = ERROR_EXPECTED_CLOSE_PAREN;
        parser->error_info.code.start_index = start_index;
        parser->error_info.code.end_index   = parser->token.end_index;
        return false;
    }

    if (!parser_consume_token(parser)) {
        return false;
    }

    return true;
}

void parser_destroy(struct Parser *parser) {
    parser->args  = NULL;
    parser->argc  = 0;
    parser->state = PARSER_DONE;
    parser->error = ERROR_NONE;
    parser->error_info.code.start_index = 0;
    parser->error_info.code.end_index   = 0;
    parser->code = NULL;
    parser->code_size = 0;
    parser->index = 0;

    ast_destroy(&parser->ast);
    buffer_destroy(&parser->buffer);
}

const char *get_error_message(enum ParserError error) {
    switch (error) {
        case ERROR_NONE:                 return "no error";
        case ERROR_ILLEGAL_CHARACTER:    return "illegal character";
        case ERROR_ILLEGAL_TOKEN:        return "illegal token";
        case ERROR_EXPECTED_CLOSE_PAREN: return "expected ')'";
        case ERROR_ILLEGAL_ARG_NAME:     return "illegal argument name";
        case ERROR_DUPLICATED_ARG_NAME:  return "duplicated argument name";
        case ERROR_UNDEFINED_VARIABLE:   return "undefined variable";
        case ERROR_OUT_OF_MEMORY:        return "out of memory";
        case ERROR_VALUE_OUT_OF_RANGE:   return "value out of range";
        case ERROR_DIV_BY_ZERO:          return "division by zero";
        default:
            assert(false);
            return "illegal error code";
    }
}

void parser_print_error(const struct Parser *parser, FILE *stream) {
    switch (parser->error) {
        case ERROR_NONE:
            fprintf(stream, "no error\n");
            return;

        case ERROR_ILLEGAL_ARG_NAME:
        case ERROR_DUPLICATED_ARG_NAME:
            fprintf(stream, "Error: %s: %s\n",
                get_error_message(parser->error),
                parser->args[parser->error_info.arg_index]);
            return;

        default:
            break;
    }

    const size_t start_index = parser->error_info.code.start_index;
    const size_t end_index   = parser->error_info.code.end_index;

    const struct Location start_loc = get_location(parser->code, parser->code_size, start_index);
    const struct Location end_loc   = get_location(parser->code, parser->code_size, end_index);
    const size_t line_start = get_line_start(parser->code, parser->code_size, start_index);

    fprintf(stream, "Error in line %zu in column %zu: %s",
        start_loc.lineno, start_loc.column,
        get_error_message(parser->error));

    if (parser->error == ERROR_ILLEGAL_TOKEN) {
        fprintf(stream, " %s", get_token_name(parser->token.type));
    } else if (parser->error == ERROR_EXPECTED_CLOSE_PAREN) {
        fprintf(stream, ", but got %s", get_token_name(parser->token.type));
    }
    fprintf(stream, "\n\n");

    const size_t padding_length = get_number_length(end_loc.lineno);

    size_t index = line_start;
    for (size_t lineno = start_loc.lineno; lineno <= end_loc.lineno; ++ lineno) {
        const size_t line_end = get_line_end(parser->code, parser->code_size, index);
        fprintf(stream, " %*zu | ", (int)padding_length, lineno);
        fwrite(parser->code + index, line_end - index, 1, stream);
        fprintf(stream, "\n ");
        for (size_t pad = 0; pad < padding_length; ++ pad) {
            fputc(' ', stream);
        }
        fprintf(stream, " | ");

        const size_t start_column = lineno == start_loc.lineno ? start_loc.column - 1 : 0;
        const size_t end_column   = lineno == end_loc.lineno   ? end_loc.column - 1 : line_end;

        size_t cursor = 0;
        for (; cursor < start_column; ++ cursor) {
            fputc(' ', stream);
        }
        if (cursor == end_column) {
            fputc('^', stream);
        } else {
            for (; cursor < end_column; ++ cursor) {
                fputc('^', stream);
            }
        }
        fputc('\n', stream);

        index = line_end + 1;
    }
}

const char *get_token_name(enum TokenType token_type) {
    switch (token_type) {
    case TOK_PLUS:        return "'+'";
    case TOK_MINUS:       return "'-'";
    case TOK_MUL:         return "'*'";
    case TOK_DIV:         return "'/'";
    case TOK_PAREN_OPEN:  return "'('";
    case TOK_PAREN_CLOSE: return "')'";
    case TOK_INT:         return "<integer>";
    case TOK_IDENT:       return "<identifier>";
    case TOK_EOF:         return "<end of file>";

    default:
        assert(false);
        return "<illegal token type>";
    }
}

bool is_identifier(const char *str) {
    char sym = *str;

    if (!IS_IDENT_HEAD(sym)) {
        return false;
    }

    for (++ str; *str; ++ str) {
        sym = *str;
        if (!IS_IDENT_TAIL(sym)) {
            return false;
        }
    }

    return true;
}
