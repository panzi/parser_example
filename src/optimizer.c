#include "optimizer.h"

#include <assert.h>

static void node_optimize(struct Ast *ast, size_t node_index);

void optimize(struct Ast *ast) {
    size_t node_index = AST_ROOT_NODE_INDEX(ast);
    node_optimize(ast, node_index);
}

// TODO: deeper optimizations
void node_optimize(struct Ast *ast, size_t node_index) {
    struct AstNode *node = &ast->nodes[node_index];

    switch (node->type) {
        case NODE_ADD:
        case NODE_SUB:
        {
            node_optimize(ast, node->binary.left_index);
            node_optimize(ast, node->binary.right_index);

            struct AstNode *left  = &ast->nodes[node->binary.left_index];
            struct AstNode *right = &ast->nodes[node->binary.right_index];

            if (left->type == NODE_INT && right->type == NODE_INT) {
                *node = (struct AstNode) {
                    .type = NODE_INT,
                    .value = node->type == NODE_ADD ?
                        left->value + right->value :
                        left->value - right->value,
                };
            } else if (left->type == NODE_INT && left->value == 0) {
                *node = (struct AstNode) {
                    .type = NODE_INT,
                    .value = node->type == NODE_SUB ? -right->value : right->value,
                };
            } else if (right->type == NODE_INT && right->value == 0) {
                *node = *left;
            }
            return;
        }
        case NODE_MUL:
        case NODE_DIV:
        {
            node_optimize(ast, node->binary.left_index);
            node_optimize(ast, node->binary.right_index);

            struct AstNode *left  = &ast->nodes[node->binary.left_index];
            struct AstNode *right = &ast->nodes[node->binary.right_index];

            if (node->type == NODE_MUL && left->type == NODE_INT && right->type == NODE_INT) {
                *node = (struct AstNode) {
                    .type = NODE_INT,
                    .value = left->value * right->value,
                };
            } else if (node->type == NODE_DIV && left->type == NODE_INT && right->type == NODE_INT && right->value != 0) {
                *node = (struct AstNode) {
                    .type = NODE_INT,
                    .value = left->value / right->value,
                };
            } else if (left->type == NODE_INT && left->value == 0) {
                *node = (struct AstNode) {
                    .type = NODE_INT,
                    .value = 0,
                };
            } else if (node->type == NODE_MUL && right->type == NODE_INT && right->value == 0) {
                *node = (struct AstNode) {
                    .type = NODE_INT,
                    .value = 0,
                };
            } else if (node->type == NODE_MUL && right->type == NODE_INT && right->value == 1) {
                *node = *left;
            } else if (node->type == NODE_MUL && left->type == NODE_INT && left->value == 1) {
                *node = *right;
            }
            return;
        }
        case NODE_INV:
        {
            node_optimize(ast, node->child_index);
            struct AstNode *child = &ast->nodes[node->child_index];
            if (child->type == NODE_INT) {
                *node = (struct AstNode) {
                    .type = NODE_INT,
                    .value = -child->value,
                };
            }
            return;
        }
        case NODE_INT:
        case NODE_VAR:
            return;

        default:
            assert(false);
    }
}
