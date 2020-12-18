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
    size_t index;

    union {
        const char *name;
        long value;
    };
};

struct Location {
    size_t lineno;
    size_t column;
};

enum ParserState {
    PARSER_TOKEN_PENDING,
    PARSER_TOKEN_READY,
    PARSER_DONE,
    PARSER_ERROR,
};

enum ParserError {
    ERROR_NONE,
    ERROR_ILLEGAL_CHARACTER,
    ERROR_ILLEGAL_TOKEN,
    ERROR_ILLEGAL_ARG_NAME,
    ERROR_DUPLICATED_ARG_NAME,
    ERROR_UNDEFINED_VARIABLE,
    ERROR_OUT_OF_MEMORY,
    ERROR_VALUE_OUT_OF_RANGE,
    ERROR_DIV_BY_ZERO,
};

typedef const char * ParserStr;

struct Parser {
    char *const * args;
    size_t argc;

    enum ParserState state;
    enum ParserError error;
    size_t error_index;

    const char *code;
    size_t code_size;
    size_t index;

    struct Buffer buffer;
    struct Token token;

    struct Ast ast;
};

struct Location get_location(const char *code, size_t size, size_t index);

struct Parser parse_slice(const char *code, size_t code_size, char *const *const args, size_t argc);
struct Parser parse_string(const char *code, char *const *const args, size_t argc);

void parser_print_error(const struct Parser *parser, FILE *stream);

const char *get_error_message(enum ParserError error);
const char *get_token_name(enum TokenType token_type);

void parser_destroy(struct Parser *parser);

bool is_identifier(const char *str);

#define IS_IDENT_HEAD(SYM) (((SYM) >= 'a' && (SYM) <= 'z') || ((SYM) >= 'A' && (SYM) <= 'Z') || (SYM) == '_')
#define IS_IDENT_TAIL(SYM) (IS_IDENT_HEAD(SYM) || ((SYM) >= '0' && (SYM) <= '9'))

#ifdef __cplusplus
}
#endif

#endif
