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

static bool parse_expr(struct Parser *parser);
static bool parse_add_sub(struct Parser *parser, struct AstNode *node);
static bool parse_mul_div(struct Parser *parser, struct AstNode *node);
static bool parse_signed(struct Parser *parser, struct AstNode *node);
static bool parse_atom(struct Parser *parser, struct AstNode *node);
static bool parse_paren(struct Parser *parser, struct AstNode *node);

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

bool parse_token(struct Parser *parser) {
    parser_skip_ignoreable(parser);

    if (parser->index >= parser->code_size) {
        parser->state = PARSER_TOKEN_READY;
        parser->token.type = TOK_EOF;
        parser->token.index = parser->index;
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
            parser->token.index = parser->index;
            parser->state = PARSER_TOKEN_READY;
            parser->token.type = (enum TokenType) sym;
            ++ parser->index;
            return true;

        default:
            if (sym >= '0' && sym <= '9') {
                parser->token.index = parser->index;
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
                        parser->error_index = parser->token.index;
                        return false;
                    }

                    value *= 10;
                    value += digit;

                    ++ parser->index;
                }
                parser->state = PARSER_TOKEN_READY;
                parser->token.type = TOK_INT;
                parser->token.value = value;

                return true;
            } else if ((sym >= 'a' && sym <= 'z') ||
                       (sym >= 'A' && sym <= 'Z') ||
                       sym == '_') {
                parser->token.index = parser->index;

                while (parser->index < parser->code_size) {
                    sym = parser->code[parser->index];

                    if (!((sym >= 'a' && sym <= 'z') ||
                          (sym >= 'A' && sym <= 'Z') ||
                          (sym >= '0' && sym <= '9') ||
                          sym == '_')) {
                        break;
                    }

                    ++ parser->index;
                }

                const size_t namelen = parser->index - parser->token.index;
                size_t name_offset = parser->ast.buffer.used;

                if (!buffer_append(&parser->ast.buffer, parser->code + parser->token.index, namelen)) {
                    parser->state = PARSER_ERROR;
                    parser->error = ERROR_OUT_OF_MEMORY;
                    parser->error_index = parser->token.index;
                    return false;
                }

                if (!buffer_append_byte(&parser->ast.buffer, 0)) {
                    parser->state = PARSER_ERROR;
                    parser->error = ERROR_OUT_OF_MEMORY;
                    parser->error_index = parser->token.index;
                    return false;
                }

                // de-duplicate symbols
                // This is an inefficient linear search, but we can't simply reorder (sort) the strings
                const char *name = parser->ast.buffer.data + name_offset;
                for (size_t other_offset = 0; other_offset < name_offset; ) {
                    const char *othername = parser->ast.buffer.data + other_offset;
                    if (strcmp(othername, name) == 0) {
                        parser->ast.buffer.used = name_offset;
                        name_offset = other_offset;
                        break;
                    }
                    other_offset += strlen(othername) + 1;
                }

                parser->state = PARSER_TOKEN_READY;
                parser->token.type = TOK_IDENT;
                parser->token.name_offset = name_offset;

                return true;
            } else {
                parser->state = PARSER_ERROR;
                parser->error = ERROR_ILLEGAL_CHARACTER;
                parser->error_index = parser->index;

                return false;
            }
            
    }
}

struct Parser parser_create_from_string(const char *code) {
    return parser_create_from_slice(code, strlen(code));
}

struct Parser parser_create_from_slice(const char *code, size_t code_size) {
    struct Parser parser = {
        .state = PARSER_TOKEN_PENDING,
        .error = ERROR_NONE,
        .error_index = 0,
        .code = code,
        .code_size = code_size,
        .index = 0,
        .token = {
            .type = TOK_EOF,
        },
        .ast = AST_INIT,
    };

    if (!parse_expr(&parser)) {
        return parser;
    }

    if (!parser_peek_token(&parser)) {
        return parser;
    }

    if (parser.token.type != TOK_EOF) {
        parser.state = PARSER_ERROR;
        parser.error = ERROR_ILLEGAL_TOKEN;
        parser.error_index = parser.token.index;
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
        parser->error_index = node->code_index;
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
        const size_t code_index = parser->token.index;

        if (token_type != TOK_PLUS && token_type != TOK_MINUS) {
            break;
        }

        if (!parser_append_node(parser, node) || !parser_consume_token(parser)) {
            return false;
        }

        const size_t left_index = AST_LAST_NODE_INDEX(&parser->ast);
        struct AstNode right;

        if (!parse_mul_div(parser, &right)) {
            return false;
        }

        if (node->type == NODE_INT && right.type == NODE_INT) {
            // constant folding
            *node = (struct AstNode) {
                .type = NODE_INT,
                .code_index = code_index,
                .value = token_type == TOK_PLUS ?
                    node->value + right.value :
                    node->value - right.value
            };
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else if (node->type == NODE_INT && node->value == 0) {
            // constant folding
            *node = right;
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else if (right.type == NODE_INT && right.value == 0) {
            // constant folding
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else {
            if (!parser_append_node(parser, &right)) {
                return false;
            }
            const size_t right_index = AST_LAST_NODE_INDEX(&parser->ast);

            *node = (struct AstNode) {
                .type = (enum NodeType) token_type,
                .code_index = code_index,
                .binary = {
                    .left_index  = left_index,
                    .right_index = right_index,
                }
            };
        }
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
        const size_t code_index = parser->token.index;

        if (token_type != TOK_MUL && token_type != TOK_DIV) {
            break;
        }

        if (!parser_append_node(parser, node) || !parser_consume_token(parser)) {
            return false;
        }

        const size_t left_index = AST_LAST_NODE_INDEX(&parser->ast);
        struct AstNode right;

        if (!parse_signed(parser, &right)) {
            return false;
        }

        if (token_type == TOK_DIV && right.type == NODE_INT && right.value == 0) {
            parser->state = PARSER_ERROR;
            parser->error = ERROR_DIV_BY_ZERO;
            parser->error_index = right.code_index;
            return false;
        }

        if (node->type == NODE_INT && right.type == NODE_INT) {
            // constant folding
            *node = (struct AstNode) {
                .type = NODE_INT,
                .code_index = code_index,
                .value = token_type == TOK_MUL ?
                    node->value * right.value :
                    node->value / right.value,
            };
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else if (node->type == NODE_INT && node->value == 1) {
            // constant folding
            *node = right;
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else if (right.type == NODE_INT && right.value == 1) {
            // constant folding
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else if (right.type == NODE_INT && token_type == TOK_MUL && right.value == 0) {
            // constant folding
            *node = (struct AstNode) {
                .type = NODE_INT,
                .code_index = code_index,
                .value = node->value,
            };
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else if (node->type == NODE_INT && token_type == TOK_MUL && node->value == 0) {
            // constant folding
            *node = (struct AstNode) {
                .type = NODE_INT,
                .code_index = code_index,
                .value = right.value,
            };
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else if (node->type == NODE_INT && token_type == TOK_DIV && node->value == 0) {
            // constant folding
            *node = (struct AstNode) {
                .type = NODE_INT,
                .code_index = code_index,
                .value = 0,
            };
            assert(parser->ast.nodes_used > 0);
            -- parser->ast.nodes_used;
        } else {
            if (!parser_append_node(parser, &right)) {
                return false;
            }
            const size_t right_index = AST_LAST_NODE_INDEX(&parser->ast);

            *node = (struct AstNode) {
                .type = (enum NodeType) token_type,
                .code_index = code_index,
                .binary = {
                    .left_index  = left_index,
                    .right_index = right_index,
                }
            };
        }
    }

    return true;
}

bool parse_signed(struct Parser *parser, struct AstNode *node) {
    bool inverse = false;

    if (!parser_peek_token(parser)) {
        return false;
    }

    const size_t code_index = parser->token.index;

    for (;;) {
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
            // constant folding
            if (child.value == LONG_MAX) {
                parser->state = PARSER_ERROR;
                parser->error = ERROR_VALUE_OUT_OF_RANGE;
                parser->error_index = code_index;
                return false;
            }

            *node = (struct AstNode) {
                .type  = NODE_INT,
                .code_index = code_index,
                .value = -child.value,
            };
            return true;
        }

        if (!parser_append_node(parser, &child)) {
            return false;
        }

        *node = (struct AstNode) {
            .type        = NODE_INV,
            .code_index  = code_index,
            .child_index = AST_LAST_NODE_INDEX(&parser->ast),
        };

        return true;
    } else {
        return parse_atom(parser, node);
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
            *node = (struct AstNode) {
                .type        = NODE_VAR,
                .code_index  = parser->token.index,
                .name_offset = parser->token.name_offset,
            };
            return true;

        case TOK_INT:
            if (!parser_consume_token(parser)) {
                return false;
            }
            *node = (struct AstNode) {
                .type       = NODE_INT,
                .code_index = parser->token.index,
                .value      = parser->token.value,
            };
            return true;

        case TOK_PAREN_OPEN:
            return parse_paren(parser, node);

        default:
            parser->state = PARSER_ERROR;
            parser->error = ERROR_ILLEGAL_TOKEN;
            parser->error_index = parser->token.index;
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
        parser->error_index = parser->token.index;
        return false;
    }

    if (!parser_consume_token(parser)) {
        return false;
    }

    if (!parse_add_sub(parser, node)) {
        return false;
    }

    if (parser->token.type != TOK_PAREN_CLOSE) {
        parser->state = PARSER_ERROR;
        parser->error = ERROR_ILLEGAL_TOKEN;
        parser->error_index = parser->token.index;
        return false;
    }

    if (!parser_consume_token(parser)) {
        return false;
    }

    return true;
}

void parser_destroy(struct Parser *parser) {
    parser->state = PARSER_DONE;
    parser->error = ERROR_NONE;
    parser->error_index = 0,
    parser->code = NULL;
    parser->code_size = 0;
    parser->index = 0;

    ast_destroy(&parser->ast);
}

const char *get_error_message(enum ParserError error) {
    switch (error) {
        case ERROR_NONE:               return "no error";
        case ERROR_ILLEGAL_CHARACTER:  return "illegal character";
        case ERROR_ILLEGAL_TOKEN:      return "illegal token";
        case ERROR_OUT_OF_MEMORY:      return "out of memory";
        case ERROR_VALUE_OUT_OF_RANGE: return "value out of range";
        case ERROR_DIV_BY_ZERO:        return "division by zero";
        default:
            assert(false);
            return "illegal error code";
    }
}

void parser_print_error(const struct Parser *parser, FILE *stream) {
    if (parser->error == ERROR_NONE) {
        fprintf(stream, "no error\n");
        return;
    }

    const size_t index = parser->error_index;
    struct Location loc = get_location(parser->code, parser->code_size, index);
    const char *lineptr = memrchr(parser->code, '\n', index + 1);
    lineptr = lineptr == NULL ? parser->code : lineptr + 1;
    const char *lineend = memchr(parser->code + index, '\n', parser->code_size - index);
    if (lineend == NULL) {
        lineend = parser->code + parser->code_size;
    }

    fprintf(stream, "Error in line %zu in column %zu: %s",
        loc.lineno, loc.column,
        get_error_message(parser->error));

    if (parser->error == ERROR_ILLEGAL_TOKEN) {
        fprintf(stream, " %s", get_token_name(parser->token.type));
    }

    fprintf(stream, "\n\n %zu | ", loc.lineno);
    fwrite(lineptr, lineend - lineptr, 1, stream);

    if (loc.lineno == 0) {
        assert(false);
        fprintf(stream, "\n  ");
    } else {
        fprintf(stream, "\n ");

        size_t pad = loc.lineno;
        while (pad > 0) {
            fputc(' ', stream);
            pad /= 10;
        }
    }

    fprintf(stream, " | ");

    for (size_t dash_count = loc.column - 1; dash_count > 0; -- dash_count) {
        fputc('-', stream);
    }
    fprintf(stream, "^\n");
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
