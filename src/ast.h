#ifndef AST_H
#define AST_H
#pragma once

#include <stdio.h>
#include <stdbool.h>

#include "buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

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

struct Ast {
    // node storage
    struct AstNode *nodes;
    size_t nodes_used;
    size_t nodes_capacity;

    // string storage
    struct Buffer buffer;
};

bool ast_append_node(struct Ast *ast, const struct AstNode *node);
void ast_print(const struct Ast *ast, FILE *stream);
long ast_eval(const struct Ast *ast);
void ast_destroy(struct Ast *ast);

#define AST_LAST_NODE_INDEX(AST) ((AST)->nodes_used - 1)
#define AST_INIT { \
        .nodes = NULL, \
        .nodes_used = 0, \
        .nodes_capacity = 0, \
        .buffer = BUFFER_INIT, \
    }

#ifdef __cplusplus
}
#endif

#endif
