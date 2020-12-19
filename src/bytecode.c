#include "bytecode.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

// This bytecode is endian dependant!
bool bytecode_write_int(struct Buffer *buffer, long value) {
    return buffer_append(buffer, (const char*)&value, sizeof(value));
}

bool bytecode_write_size(struct Buffer *buffer, size_t value) {
    return buffer_append(buffer, (const char*)&value, sizeof(value));
}

enum ByteCode {
    CODE_ADD,
    CODE_SUB,
    CODE_MUL,
    CODE_DIV,
    CODE_INV,
    CODE_VAL,
    CODE_VAR,
    CODE_RET,
};

bool node_compile(struct Bytecode *bytecode, const struct Ast *ast, size_t node_index, size_t stack_size) {
    assert(node_index < ast->nodes_used);

    const struct AstNode *node = &ast->nodes[node_index];
    const size_t result_stack_size = stack_size + 1;

    switch (node->type) {
        case NODE_ADD:
            if (!node_compile(bytecode, ast, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, ast, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_ADD)) {
                return false;
            }
            break;

        case NODE_SUB:
            if (!node_compile(bytecode, ast, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, ast, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_SUB)) {
                return false;
            }
            break;

        case NODE_MUL:
            if (!node_compile(bytecode, ast, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, ast, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_MUL)) {
                return false;
            }
            break;

        case NODE_DIV:
            if (!node_compile(bytecode, ast, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, ast, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_DIV)) {
                return false;
            }
            break;

        case NODE_INV:
            if (!node_compile(bytecode, ast, node->child_index, stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_INV)) {
                return false;
            }
            break;

        case NODE_INT:
            if (!bytecode_write_int(&bytecode->bytes, CODE_VAL)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, node->value)) {
                return false;
            }
            break;

        case NODE_VAR:
            if (!bytecode_write_int(&bytecode->bytes, CODE_VAR)) {
                return false;
            }
            if (!bytecode_write_size(&bytecode->bytes, node->arg_index)) {
                return false;
            }
            break;

        default:
            assert(false);
            return false;
    }

    if (result_stack_size > bytecode->stack_size) {
        bytecode->stack_size = result_stack_size;
    }

    return true;
}

struct Bytecode bytecode_compile(const struct Ast *ast) {
    struct Bytecode bytecode = { .bytes = BUFFER_INIT, .stack_size = 0 };

    // stack size placeholder
    if (!bytecode_write_size(&bytecode.bytes, 0)) {
        goto error;
    }

    // generate bytecode
    if (!node_compile(&bytecode, ast, AST_ROOT_NODE_INDEX(ast), 0)) {
        goto error;
    }

    if (!bytecode_write_int(&bytecode.bytes, CODE_RET)) {
        goto error;
    }

    // fill in stack size
    memcpy(bytecode.bytes.data, &bytecode.stack_size, sizeof(size_t));

    goto end;

error:
    // TODO: better error handling
    bytecode.stack_size = 0;

end:

    return bytecode;
}

long bytecode_eval(const void *bytecode, const long args[]) {
    long *stack = malloc(sizeof(long) * *(const size_t*)bytecode);
    if (stack == NULL) {
        // TODO: better error handling
        perror("allocating varialbe stack");
        return LONG_MAX;
    }

    // non-standard address from label for faster interpreter loop
    // This feature is supported by GCC and LLVM.
    static const void *table[] = {
        [CODE_ADD] = &&add,
        [CODE_SUB] = &&sub,
        [CODE_MUL] = &&mul,
        [CODE_DIV] = &&div,
        [CODE_INV] = &&inv,
        [CODE_VAL] = &&val,
        [CODE_VAR] = &&var,
        [CODE_RET] = &&ret,
    };

    const void *codeptr = bytecode + sizeof(size_t);
    long *stackptr = stack;

    goto *table[*(const long*)codeptr];

add:
    -- stackptr;
    stackptr[-1] += *stackptr;
    codeptr += sizeof(long);
    goto *table[*(const long*)codeptr];

sub:
    -- stackptr;
    stackptr[-1] -= *stackptr;
    codeptr += sizeof(long);
    goto *table[*(const long*)codeptr];

mul:
    -- stackptr;
    stackptr[-1] *= *stackptr;
    codeptr += sizeof(long);
    goto *table[*(const long*)codeptr];

div:
    -- stackptr;
    stackptr[-1] /= *stackptr;
    codeptr += sizeof(long);
    goto *table[*(const long*)codeptr];

inv:
    stackptr[-1] = -stackptr[-1];
    codeptr += sizeof(long);
    goto *table[*(const long*)codeptr];

val:
    codeptr += sizeof(long);
    *stackptr = *(const long*)codeptr;
    ++ stackptr;
    codeptr += sizeof(long);
    goto *table[*(const long*)codeptr];

var:
    codeptr += sizeof(long);
    const size_t arg_index = *(const size_t*)codeptr;
    *stackptr = args[arg_index];
    ++ stackptr;
    codeptr += sizeof(size_t);
    goto *table[*(const long*)codeptr];

ret:
    -- stackptr;
    const long result = *stackptr;
    free(stack);
    return result;
}

void bytecode_destroy(struct Bytecode *bytecode) {
    buffer_destroy(&bytecode->bytes);
    bytecode->stack_size = 0;
}

void bytecode_print(const void *bytecode, char *const *const args, FILE *stream) {
    const size_t stack_size = sizeof(long) * *(const size_t*)bytecode;
    const void *codeptr = bytecode + sizeof(long);

    fprintf(stdout, "stack size: %zu cells (%zu B)\n\n", stack_size / sizeof(long), stack_size);

    for (;;) {
        const long code = *(const long*)codeptr;
        codeptr += sizeof(long);

        switch (code) {
            case CODE_ADD:
                fprintf(stream, "ADD\n");
                break;

            case CODE_SUB:
                fprintf(stream, "SUB\n");
                break;

            case CODE_MUL:
                fprintf(stream, "MUL\n");
                break;

            case CODE_DIV:
                fprintf(stream, "DIV\n");
                break;

            case CODE_INV:
                fprintf(stream, "INV\n");
                break;

            case CODE_VAL:
                fprintf(stream, "VAL %ld\n", *(const long*)codeptr);
                codeptr += sizeof(long);
                break;

            case CODE_VAR:
            {
                const size_t arg_index = *(const size_t*)codeptr;
                codeptr += sizeof(size_t);
                fprintf(stream, "VAR %s\n", args[arg_index]);
                break;
            }

            case CODE_RET:
                fprintf(stream, "RET\n");
                return;
        }
    }
}
