#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "ast.h"

static void node_print(const struct Ast *ast, size_t node_index, FILE *stream);
static long node_eval(const struct Ast *ast, size_t node_index);

void ast_destroy(struct Ast *ast) {
    free(ast->nodes);
    ast->nodes = NULL;
    ast->nodes_capacity = 0;
    ast->nodes_used = 0;

    buffer_destroy(&ast->buffer);
}

long ast_eval(const struct Ast *ast) {
    if (ast->nodes_used == 0) {
        return 0;
    }
    return node_eval(ast, AST_LAST_NODE_INDEX(ast));
}

void ast_print(const struct Ast *ast, FILE *stream) {
    if (ast->nodes_used == 0) {
        return;
    }
    node_print(ast, AST_LAST_NODE_INDEX(ast), stream);
}

long node_eval(const struct Ast *ast, size_t node_index) {
    struct AstNode *node = &ast->nodes[node_index];
    switch (node->type) {
        case NODE_ADD:
        {
            const long left  = node_eval(ast, node->binary.left_index);
            const long right = node_eval(ast, node->binary.right_index);
            return left + right;
        }

        case NODE_SUB:
        {
            const long left  = node_eval(ast, node->binary.left_index);
            const long right = node_eval(ast, node->binary.right_index);
            return left - right;
        }

        case NODE_MUL:
        {
            const long left  = node_eval(ast, node->binary.left_index);
            const long right = node_eval(ast, node->binary.right_index);
            return left * right;
        }

        case NODE_DIV:
        {
            const long left  = node_eval(ast, node->binary.left_index);
            const long right = node_eval(ast, node->binary.right_index);
            return left / right;
        }

        case NODE_INV:
        {
            const long value = node_eval(ast, node->child_index);
            return -value;
        }

        case NODE_INT:
            return node->value;

        case NODE_VAR:
        {
            const char *name = ast->buffer.data + node->name_offset;
            const char *strvalue = getenv(name);
            if (strvalue == NULL) {
                fprintf(stderr, "WARNING: Environment variable %s is not set!\n", name);
                return 0;
            }

            char *endptr = NULL;
            const long value = strtol(strvalue, &endptr, 10);

            if (!*strvalue || *endptr) {
                fprintf(stderr, "WARNING: Error parsing environment variable %s=%s\n", name, strvalue);
            }

            return value;
        }

        default:
            assert(false);
            return 0;
    }
}

void node_print(const struct Ast *ast, size_t node_index, FILE *stream) {
    struct AstNode *node = &ast->nodes[node_index];
    switch (node->type) {
        case NODE_ADD:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, stream);
            fprintf(stream, " + ");
            node_print(ast, node->binary.right_index, stream);
            fputc(')', stream);
            return;

        case NODE_SUB:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, stream);
            fprintf(stream, " - ");
            node_print(ast, node->binary.right_index, stream);
            fputc(')', stream);
            return;

        case NODE_MUL:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, stream);
            fprintf(stream, " * ");
            node_print(ast, node->binary.right_index, stream);
            fputc(')', stream);
            return;

        case NODE_DIV:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, stream);
            fprintf(stream, " / ");
            node_print(ast, node->binary.right_index, stream);
            fputc(')', stream);
            return;

        case NODE_INV:
        {
            fputc('-', stream);
            node_print(ast, node->child_index, stream);
            return;
        }

        case NODE_INT:
            fprintf(stream, "%ld", node->value);
            return;

        case NODE_VAR:
            fprintf(stream, "%s", ast->buffer.data + node->name_offset);
            return;

        default:
            fprintf(stderr, "illegal node type: %d %c\n", node->type, node->type);
            assert(false);
    }
}

bool ast_append_node(struct Ast *ast, const struct AstNode *node) {
    if (ast->nodes_used == ast->nodes_capacity) {
        if (ast->nodes_capacity > SIZE_MAX / 2 / sizeof(struct AstNode)) {
            return false;
        }

        const size_t new_capacity = ast->nodes_capacity == 0 ?
            64 :
            ast->nodes_capacity * 2;
        struct AstNode *new_nodes = realloc(ast->nodes, new_capacity * sizeof(struct AstNode));

        if (new_nodes == NULL) {
            return false;
        }

        ast->nodes = new_nodes;
        ast->nodes_capacity = new_capacity;
    }

    ast->nodes[ast->nodes_used] = *node;
    ++ ast->nodes_used;

    return true;
}
