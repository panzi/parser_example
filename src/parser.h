#ifndef PARSER_H
#define PARSER_H
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "buffer.h"
#include "ast.h"

/*

EXPR    := ADD_SUB
ADD_SUB := MUL_DIV {( "+" | "-" ) MUL_DIV}
MUL_DIV := SIGNED {( "*" | "/" ) SIGNED}
SIGNED  := {"+" | "-"} ATOM
ATOM    := IDENT | INT | PAREN
PAREN   := "(" EXPR ")"
INT     := [0-9]+
IDENT   := [_a-zA-Z][_a-zA-Z0-9]*

COMMENT := #.*$

*/

#ifdef __cplusplus
extern "C" {
#endif

enum TokenType {
    TOK_PLUS  = '+',
    TOK_MINUS = '-',
    TOK_MUL = '*',
    TOK_DIV = '/',
    TOK_PAREN_OPEN  = '(',
    TOK_PAREN_CLOSE = ')',
    TOK_INT = 256,
    TOK_IDENT,
    TOK_EOF = -1,
};

struct Token {
    enum TokenType type;
    size_t start_index;
    size_t end_index;

    union {
        const char *name;
        long value;
    };
};

struct Location {
    size_t lineno;
    size_t column;
};

struct Range {
    size_t start_index;
    size_t end_index;
};

enum ParserState {
    PARSER_TOKEN_PENDING,
    PARSER_TOKEN_READY,
    PARSER_DONE,
    PARSER_ERROR,
};

enum ParserError {
    ERROR_NONE,
    ERROR_ILLEGAL_CHARACTER,    // raw location
    ERROR_ILLEGAL_TOKEN,        // token
    ERROR_EXPECTED_CLOSE_PAREN, // token
    ERROR_ILLEGAL_ARG_NAME,     // args
    ERROR_DUPLICATED_ARG_NAME,  // args
    ERROR_UNDEFINED_VARIABLE,   // token
    ERROR_OUT_OF_MEMORY,        // raw location
    ERROR_VALUE_OUT_OF_RANGE,   // token or node -> raw location
    ERROR_DIV_BY_ZERO,          // node
};

struct Parser {
    char *const * args;
    size_t argc;

    enum ParserState state;
    enum ParserError error;
    union {
        size_t arg_index;
        struct Range code;
    } error_info;

    const char *code;
    size_t code_size;
    size_t index;

    struct Buffer buffer;
    struct Token token;

    struct Ast ast;
};

#define PARSER_INIT \
    { \
        .args = NULL, \
        .argc = 0, \
        .state = PARSER_DONE, \
        .error = ERROR_NONE, \
        .error_info = { \
            .code = { \
                .start_index = 0, \
                .end_index   = 0, \
            } \
        }, \
        .code = NULL, \
        .code_size = 0, \
        .index = 0, \
        .token = { \
            .type = TOK_EOF, \
        }, \
        .ast = AST_INIT, \
        .buffer = BUFFER_INIT, \
    }

struct Location get_location(const char *code, size_t size, size_t index);
struct Range get_line_range(const char *code, size_t size, size_t index);
size_t get_line_start(const char *code, size_t size, size_t index);
size_t get_line_end(const char *code, size_t size, size_t index);

struct Parser parse_slice(const char *code, size_t code_size, char *const *const args, size_t argc);
struct Parser parse_string(const char *code, char *const *const args, size_t argc);

void parser_print_error(const struct Parser *parser, FILE *stream);

const char *get_parser_state_name(enum ParserState state);
const char *get_parser_error_message(enum ParserError error);
const char *get_token_name(enum TokenType token_type);

void parser_destroy(struct Parser *parser);

bool is_identifier(const char *str);

#define IS_IDENT_HEAD(SYM) (((SYM) >= 'a' && (SYM) <= 'z') || ((SYM) >= 'A' && (SYM) <= 'Z') || (SYM) == '_')
#define IS_IDENT_TAIL(SYM) (IS_IDENT_HEAD(SYM) || ((SYM) >= '0' && (SYM) <= '9'))

#ifdef __cplusplus
}
#endif

#endif
