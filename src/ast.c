#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

#include "ast.h"

static void node_print(const struct Ast *ast, size_t node_index, char *const *const args, FILE *stream);
static long node_eval(const struct Ast *ast, size_t node_index, const long args[]);

void ast_destroy(struct Ast *ast) {
    free(ast->nodes);
    ast->nodes = NULL;
    ast->nodes_capacity = 0;
    ast->nodes_used = 0;
}

long ast_eval(const struct Ast *ast, const long args[]) {
    if (ast->nodes_used == 0) {
        return 0;
    }
    return node_eval(ast, AST_ROOT_NODE_INDEX(ast), args);
}

void ast_print(const struct Ast *ast, char *const *const args, FILE *stream) {
    if (ast->nodes_used == 0) {
        return;
    }
    node_print(ast, AST_ROOT_NODE_INDEX(ast), args, stream);
}

long node_eval(const struct Ast *ast, size_t node_index, const long args[]) {
    assert(node_index < ast->nodes_used);

    struct AstNode *node = &ast->nodes[node_index];
    switch (node->type) {
        case NODE_ADD:
        {
            const long left  = node_eval(ast, node->binary.left_index, args);
            const long right = node_eval(ast, node->binary.right_index, args);
            return left + right;
        }

        case NODE_SUB:
        {
            const long left  = node_eval(ast, node->binary.left_index, args);
            const long right = node_eval(ast, node->binary.right_index, args);
            return left - right;
        }

        case NODE_MUL:
        {
            const long left  = node_eval(ast, node->binary.left_index, args);
            const long right = node_eval(ast, node->binary.right_index, args);
            return left * right;
        }

        case NODE_DIV:
        {
            const long left  = node_eval(ast, node->binary.left_index, args);
            const long right = node_eval(ast, node->binary.right_index, args);
            return left / right;
        }

        case NODE_INV:
        {
            const long value = node_eval(ast, node->child_index, args);
            return -value;
        }

        case NODE_INT:
            return node->value;

        case NODE_VAR:
            return args[node->arg_index];

        default:
            assert(false);
            return 0;
    }
}

void node_print(const struct Ast *ast, size_t node_index, char *const *const args, FILE *stream) {
    assert(node_index < ast->nodes_used);

    struct AstNode *node = &ast->nodes[node_index];
    switch (node->type) {
        case NODE_ADD:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, args, stream);
            fprintf(stream, " + ");
            node_print(ast, node->binary.right_index, args, stream);
            fputc(')', stream);
            return;

        case NODE_SUB:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, args, stream);
            fprintf(stream, " - ");
            node_print(ast, node->binary.right_index, args, stream);
            fputc(')', stream);
            return;

        case NODE_MUL:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, args, stream);
            fprintf(stream, " * ");
            node_print(ast, node->binary.right_index, args, stream);
            fputc(')', stream);
            return;

        case NODE_DIV:
            fputc('(', stream);
            node_print(ast, node->binary.left_index, args, stream);
            fprintf(stream, " / ");
            node_print(ast, node->binary.right_index, args, stream);
            fputc(')', stream);
            return;

        case NODE_INV:
        {
            fputc('-', stream);
            node_print(ast, node->child_index, args, stream);
            return;
        }

        case NODE_INT:
            fprintf(stream, "%ld", node->value);
            return;

        case NODE_VAR:
            fprintf(stream, "%s", args[node->arg_index]);
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
