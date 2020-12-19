#include "optimizer.h"

#include <assert.h>

static void node_optimize(struct Ast *ast, size_t node_index);

void optimize(struct Ast *ast) {
    size_t node_index = AST_ROOT_NODE_INDEX(ast);
    node_optimize(ast, node_index);
}

// TODO: deeper optimizations
void node_optimize(struct Ast *ast, const size_t node_index) {
    struct AstNode *node = &ast->nodes[node_index];

    // first optimize sub-trees
    switch (node->type) {
        case NODE_ADD:
        case NODE_SUB:
        case NODE_MUL:
        case NODE_DIV:
            node_optimize(ast, node->binary.left_index);
            node_optimize(ast, node->binary.right_index);
            break;

        case NODE_INV:
            node_optimize(ast, node->child_index);
            break;

        case NODE_INT:
        case NODE_VAR:
            break;
    }

    // A loop is needed because optimizations change the tree in a way that
    // might make another optimization to be applicable. However, we ensure
    // that the sub-trees stay optimized so we don't have to do that recursive
    // part again.
    for (;;) {
        const enum NodeType type = node->type;

        switch (type) {
            case NODE_ADD:
            case NODE_SUB:
            {
                struct AstNode *left  = &ast->nodes[node->binary.left_index];
                struct AstNode *right = &ast->nodes[node->binary.right_index];

                if (left->type == NODE_INT && right->type == NODE_INT) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .value = type == NODE_ADD ?
                            left->value + right->value :
                            left->value - right->value,
                    };
                } else if (left->type == NODE_INT && left->value == 0) {
                    *node = *right;
                } else if (right->type == NODE_INT && right->value == 0) {
                    if (type == NODE_ADD) {
                        *node = *left;
                    } else {
                        *node = (struct AstNode) {
                            .type = NODE_INV,
                            .child_index = node->binary.left_index,
                        };

                        continue;
                    }
                } else if (left->type == NODE_INT &&
                    (right->type == NODE_ADD || right->type == NODE_SUB) && (
                    ast->nodes[right->binary.left_index].type == NODE_INT ||
                    ast->nodes[right->binary.right_index].type == NODE_INT
                )) {
                    if (ast->nodes[right->binary.left_index].type == NODE_INT) {
                        // 1 +/- (2 +/- X) -> (1 +/- 2) +/- X
                        *node = (struct AstNode) {
                            .type = type == right->type ? NODE_ADD : NODE_SUB,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = right->binary.right_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_INT,
                            .value = type == NODE_ADD ?
                                left->value + ast->nodes[right->binary.left_index].value :
                                left->value - ast->nodes[right->binary.left_index].value,
                        };
                    } else {
                        // 1 +/- (X +/- 2) -> (1 +/- 2) +/- X
                        *node = (struct AstNode) {
                            .type = type,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = right->binary.left_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_INT,
                            .value = type == right->type ?
                                left->value + ast->nodes[right->binary.right_index].value :
                                left->value - ast->nodes[right->binary.right_index].value,
                        };
                    }

                    continue;
                } else if (right->type == NODE_INT &&
                    (left->type == NODE_ADD || left->type == NODE_SUB) && (
                    ast->nodes[left->binary.left_index].type == NODE_INT ||
                    ast->nodes[left->binary.right_index].type == NODE_INT
                )) {
                    if (ast->nodes[left->binary.right_index].type == NODE_INT) {
                        // (X +/- 1) +/- 2 -> X +/- (1 +/- 2)
                        *node = (struct AstNode) {
                            .type = NODE_ADD,
                            .binary = {
                                .left_index  = left->binary.left_index,
                                .right_index = node->binary.right_index,
                            }
                        };

                        long value = ast->nodes[left->binary.right_index].value;
                        if (left->type == NODE_SUB) {
                            value = -value;
                        }
                        value = type == NODE_ADD ?
                            value + right->value :
                            value - right->value;

                        *right = (struct AstNode) {
                            .type = NODE_INT,
                            .value = value,
                        };
                    } else {
                        // (1 +/- X) +/- 2 -> (1 +/- 2) +/- X
                        *node = (struct AstNode) {
                            .type = left->type,
                            .binary = {
                                .left_index  = node->binary.right_index,
                                .right_index = left->binary.right_index,
                            }
                        };

                        long value = ast->nodes[left->binary.left_index].value;
                        value = type == NODE_ADD ?
                            value + right->value :
                            value - right->value;

                        *right = (struct AstNode) {
                            .type = NODE_INT,
                            .value = value,
                        };
                    }

                    continue;
                } else if (right->type == NODE_ADD || right->type == NODE_SUB) {
                    // X + (C +/- D) -> (X + C) +/- D
                    // X - (C +/- D) -> (X - C) -/+ D
                    const size_t left_index = node->binary.left_index;

                    *node = (struct AstNode) {
                        .type = type == NODE_ADD ? right->type :
                            right->type == NODE_ADD ? NODE_SUB : NODE_ADD,
                        .binary = {
                            .left_index  = node->binary.right_index,
                            .right_index = right->binary.right_index,
                        }
                    };

                    *right = (struct AstNode) {
                        .type = type,
                        .binary = {
                            .left_index  = left_index,
                            .right_index = right->binary.left_index,
                        }
                    };

                    continue;
                }
                return;
            }
            case NODE_MUL:
            case NODE_DIV:
            {
                struct AstNode *left  = &ast->nodes[node->binary.left_index];
                struct AstNode *right = &ast->nodes[node->binary.right_index];

                if (type == NODE_MUL && left->type == NODE_INT && right->type == NODE_INT) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .value = left->value * right->value,
                    };
                } else if (type == NODE_DIV && left->type == NODE_INT && right->type == NODE_INT && right->value != 0) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .value = left->value / right->value,
                    };
                } else if (left->type == NODE_INT && left->value == 0) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .value = 0,
                    };
                } else if (type == NODE_MUL && right->type == NODE_INT && right->value == 0) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .value = 0,
                    };
                } else if (type == NODE_MUL && right->type == NODE_INT && right->value == 1) {
                    *node = *left;
                } else if (type == NODE_MUL && left->type == NODE_INT && left->value == 1) {
                    *node = *right;
                }
                return;
            }
            case NODE_INV:
            {
                struct AstNode *child = &ast->nodes[node->child_index];
                if (child->type == NODE_INT) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .value = -child->value,
                    };
                } else if (child->type == NODE_INV) {
                    *node = ast->nodes[node->child_index];
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
}
