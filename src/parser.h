#ifndef PARSER_H
#define PARSER_H
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "buffer.h"

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
        size_t name_offset;
        long   value;
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
    ERROR_OUT_OF_MEMORY,
    ERROR_VALUE_OUT_OF_RANGE,
    ERROR_DIV_BY_ZERO,
};

enum NodeType {
    NODE_ADD = '+',
    NODE_SUB = '-',
    NODE_MUL = '*',
    NODE_DIV = '/',
    NODE_INV = 256,
    NODE_INT,
    NODE_VAR,
};

struct AstNode {
    enum NodeType type;
    size_t code_index;
    union {
        struct {
            size_t left_index;
            size_t right_index;
        } binary;

        size_t child_index;

        long value;

        size_t name_offset;
    };
};

struct Parser {
    enum ParserState state;
    enum ParserError error;
    size_t error_index;

    const char *code;
    size_t code_size;
    size_t index;

    struct Token token;

    // node storage
    struct AstNode *nodes;
    size_t nodes_used;
    size_t nodes_capacity;

    // string storage
    struct Buffer buffer;
};

struct Location get_location(const char *code, size_t size, size_t index);

struct Parser parser_create_from_slice(const char *code, size_t code_size);
struct Parser parser_create_from_string(const char *code);

void parser_print_error(const struct Parser *parser, FILE *stream);

void parser_print_ast(const struct Parser *parser, FILE *stream);
long parser_eval_ast(const struct Parser *parser);
const char *parser_error_message(enum ParserError error);
const char *get_token_name(enum TokenType token_type);

void parser_destroy(struct Parser *parser);

#ifdef __cplusplus
}
#endif

#endif
