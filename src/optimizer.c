#include "optimizer.h"

#include <assert.h>

static void node_optimize_recursive(struct Ast *ast, size_t node_index);
static void node_optimize(struct Ast *ast, const size_t node_index);

void optimize(struct Ast *ast) {
    const size_t node_index = AST_ROOT_NODE_INDEX(ast);
    node_optimize_recursive(ast, node_index);
}

// TODO: deeper optimizations
void node_optimize_recursive(struct Ast *ast, const size_t node_index) {
    struct AstNode *node = &ast->nodes[node_index];

    // first optimize sub-trees
    switch (node->type) {
        case NODE_ADD:
        case NODE_SUB:
        case NODE_MUL:
        case NODE_DIV:
            node_optimize_recursive(ast, node->binary.left_index);
            node_optimize_recursive(ast, node->binary.right_index);
            break;

        case NODE_INV:
            node_optimize_recursive(ast, node->child_index);
            break;

        case NODE_INT:
        case NODE_VAR:
            break;
    }

    node_optimize(ast, node_index);
}

void node_optimize(struct Ast *ast, const size_t node_index) {
    // NOTE: Some of the optimizations here reorder instructions. This might
    //       cause calculations that didn't have interger overflows/underflows
    //       in intermediate results to now have them. But I think this should
    //       be fine, since in the actual result it will all cancel out with
    //       the opposite overflow/underflow again.
    // NOTE: Integer overflows/underflows are in principle undefined behavior
    //       in C, but it is well enough defined on common architectures like
    //       x86_32/x86_64.

    // TODO: Change code_index of changed nodes to something sane.

    struct AstNode *node = &ast->nodes[node_index];

    // A loop is needed because optimizations change the tree in a way that
    // might make another optimization to be applicable. However, we ensure
    // that the sub-trees stay optimized so we don't have to do that recursive
    // part again. In a few cases we do change sub-trees enough to need to
    // call node_optimize() on them again, though.
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
                        .start_index = node->start_index,
                        .end_index  = node->end_index,
                        .value = type == NODE_ADD ?
                            left->value + right->value :
                            left->value - right->value,
                    };
                } else if (left->type == NODE_INT && left->value == 0) {
                    if (type == NODE_ADD) {
                        *node = *right;
                    } else {
                        *node = (struct AstNode) {
                            .type = NODE_INV,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .child_index = node->binary.right_index,
                        };

                        continue;
                    }
                } else if (right->type == NODE_INT && right->value == 0) {
                    *node = *left;
                } else if (left->type == NODE_INT &&
                    (right->type == NODE_ADD || right->type == NODE_SUB) && (
                    ast->nodes[right->binary.left_index].type == NODE_INT ||
                    ast->nodes[right->binary.right_index].type == NODE_INT
                )) {
                    if (ast->nodes[right->binary.left_index].type == NODE_INT) {
                        // 1 +/- (2 +/- X) -> (1 +/- 2) +/- X
                        *node = (struct AstNode) {
                            .type = type == right->type ? NODE_ADD : NODE_SUB,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = right->binary.right_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_INT,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .value = type == NODE_ADD ?
                                left->value + ast->nodes[right->binary.left_index].value :
                                left->value - ast->nodes[right->binary.left_index].value,
                        };
                    } else {
                        // 1 +/- (X +/- 2) -> (1 +/- 2) +/- X
                        *node = (struct AstNode) {
                            .type = type,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = right->binary.left_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_INT,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
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
                        //
                        // (X + 1) + 2 -> X + (1 + 2)
                        // (X + 1) - 2 -> X + (1 - 2)
                        // (X - 1) + 2 -> X + (2 - 1)
                        // (X - 1) - 2 -> X - (1 + 2)
                        *node = (struct AstNode) {
                            .type = NODE_ADD,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
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
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .value = value,
                        };
                    } else {
                        // (1 +/- X) +/- 2 -> (1 +/- 2) +/- X
                        *node = (struct AstNode) {
                            .type = left->type,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
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
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
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
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .binary = {
                            .left_index  = node->binary.right_index,
                            .right_index = right->binary.right_index,
                        }
                    };

                    *right = (struct AstNode) {
                        .type = type,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .binary = {
                            .left_index  = left_index,
                            .right_index = right->binary.left_index,
                        }
                    };

                    // Did enough to that branch so that we might need to optimize it
                    // again, but not the full recursive way.
                    node_optimize(ast, node->binary.left_index);

                    continue;
                } else if (right->type != NODE_INT &&
                    (left->type == NODE_ADD || left->type == NODE_SUB) &&
                    ((ast->nodes[left->binary.left_index].type == NODE_INT && left->type == NODE_ADD) ||
                     ast->nodes[left->binary.right_index].type == NODE_INT
                )) {
                    const size_t right_index = node->binary.right_index;

                    if (ast->nodes[left->binary.left_index].type == NODE_INT) {
                        // (1 + X) +/- Y -> (X +/- Y) + 1
                        *node = (struct AstNode) {
                            .type = NODE_ADD,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = left->binary.left_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = type,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = left->binary.right_index,
                                .right_index = right_index,
                            }
                        };
                    } else {
                        // (X +/- 1) +/- Y -> (X +/- Y) +/- 1
                        *node = (struct AstNode) {
                            .type = left->type,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = left->binary.right_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = type,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = left->binary.left_index,
                                .right_index = right_index,
                            }
                        };
                    }

                    node_optimize(ast, node->binary.left_index);

                    continue;
                } else if (type == NODE_SUB && left->type == NODE_VAR && right->type == NODE_VAR && left->arg_index == right->arg_index) {
                    // X - X -> 0
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .value = 0,
                    };
                }
                return;
            }
            case NODE_MUL:
            case NODE_DIV:
            {
                // TODO: more optimizations like for addition and substraction
                struct AstNode *left  = &ast->nodes[node->binary.left_index];
                struct AstNode *right = &ast->nodes[node->binary.right_index];

                if (type == NODE_MUL && left->type == NODE_INT && right->type == NODE_INT) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .value = left->value * right->value,
                    };
                } else if (type == NODE_DIV && left->type == NODE_INT && right->type == NODE_INT && right->value != 0) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .value = left->value / right->value,
                    };
                } else if (left->type == NODE_INT && left->value == 0) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .value = 0,
                    };
                } else if (type == NODE_MUL && right->type == NODE_INT && right->value == 0) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .value = 0,
                    };
                } else if (type == NODE_MUL && right->type == NODE_INT && right->value == 1) {
                    *node = *left;
                } else if (type == NODE_MUL && left->type == NODE_INT && left->value == 1) {
                    *node = *right;
                } else if (type == NODE_DIV &&
                    left->type == NODE_VAR &&
                    right->type == NODE_VAR &&
                    left->arg_index == right->arg_index
                ) {
                    // X / X -> 1
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .value = 1,
                    };
                } else if (type == NODE_MUL &&
                    left->type == NODE_INT &&
                    right->type == NODE_MUL && (
                        ast->nodes[right->binary.left_index].type == NODE_INT ||
                        ast->nodes[right->binary.right_index].type == NODE_INT
                )) {
                    // Much more minimal optimizations than with +/- because
                    // integer division is not associative (precision loss).
                    //
                    // One could do more optimizations ensuring that any changed
                    // division is not losing precision in a different way than
                    // written in the original code (and no division by zero
                    // happens in the optimizer).
                    if (ast->nodes[right->binary.left_index].type == NODE_INT) {
                        // 2 * (3 * X) -> (2 * 3) * X
                        *node = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = right->binary.right_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_INT,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .value = left->value * ast->nodes[right->binary.left_index].value,
                        };
                    } else {
                        // 2 * (X * 3) -> (2 * 3) * X
                        *node = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = right->binary.left_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_INT,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .value = left->value * ast->nodes[right->binary.right_index].value,
                        };
                    }

                    continue;
                } else if (type == NODE_MUL &&
                    right->type == NODE_INT &&
                    left->type == NODE_MUL && (
                        ast->nodes[left->binary.left_index].type == NODE_INT ||
                        ast->nodes[left->binary.right_index].type == NODE_INT
                )) {
                    // Much more minimal optimizations than with +/- because
                    // integer division is not associative (precision loss).
                    if (ast->nodes[left->binary.right_index].type == NODE_INT) {
                        // (X * 2) * 3 -> X * (2 * 3)
                        *node = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = left->binary.left_index,
                                .right_index = node->binary.right_index,
                            }
                        };

                        *right = (struct AstNode) {
                            .type = NODE_INT,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .value = ast->nodes[left->binary.right_index].value * right->value,
                        };
                    } else {
                        // (2 * X) * 3 -> X * (2 * 3)
                        *node = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = left->binary.right_index,
                                .right_index = node->binary.right_index,
                            }
                        };

                        *right = (struct AstNode) {
                            .type = NODE_INT,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .value = ast->nodes[left->binary.left_index].value * right->value,
                        };
                    }

                    continue;
                } else if (type == NODE_MUL && right->type == NODE_MUL) {
                    // X * (C * D) -> (X * C) * D
                    const size_t left_index = node->binary.left_index;

                    *node = (struct AstNode) {
                        .type = NODE_MUL,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .binary = {
                            .left_index  = node->binary.right_index,
                            .right_index = right->binary.right_index,
                        }
                    };

                    *right = (struct AstNode) {
                        .type = type,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
                        .binary = {
                            .left_index  = left_index,
                            .right_index = right->binary.left_index,
                        }
                    };

                    // Did enough to that branch so that we might need to optimize it
                    // again, but not the full recursive way.
                    node_optimize(ast, node->binary.left_index);

                    continue;
                } else if (type == NODE_MUL &&
                    right->type != NODE_INT &&
                    left->type == NODE_MUL && (
                        ast->nodes[left->binary.left_index].type  == NODE_INT ||
                        ast->nodes[left->binary.right_index].type == NODE_INT
                )) {
                    const size_t right_index = node->binary.right_index;

                    if (ast->nodes[left->binary.left_index].type == NODE_INT) {
                        // (2 * X) * Y -> (X * Y) * 2
                        *node = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = left->binary.left_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = left->binary.right_index,
                                .right_index = right_index,
                            }
                        };
                    } else {
                        // (X * 2) * Y -> (X * Y) * 2
                        *node = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = node->binary.left_index,
                                .right_index = left->binary.right_index,
                            }
                        };

                        *left = (struct AstNode) {
                            .type = NODE_MUL,
                            .start_index = node->start_index,
                            .end_index   = node->end_index,
                            .binary = {
                                .left_index  = left->binary.left_index,
                                .right_index = right_index,
                            }
                        };
                    }

                    node_optimize(ast, node->binary.left_index);

                    continue;
                }
                return;
            }
            case NODE_INV:
            {
                struct AstNode *child = &ast->nodes[node->child_index];
                if (child->type == NODE_INT) {
                    *node = (struct AstNode) {
                        .type = NODE_INT,
                        .start_index = node->start_index,
                        .end_index   = node->end_index,
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
                return;
        }
    }
}
