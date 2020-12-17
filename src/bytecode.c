#include "bytecode.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

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

bool node_compile(struct Bytecode *bytecode, const struct Parser *parser, size_t node_index, size_t stack_size) {
    const struct AstNode *node = &parser->nodes[node_index];
    const size_t result_stack_size = stack_size + 1;

    switch (node->type) {
        case NODE_ADD:
            if (!node_compile(bytecode, parser, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, parser, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_ADD)) {
                return false;
            }
            break;

        case NODE_SUB:
            if (!node_compile(bytecode, parser, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, parser, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_SUB)) {
                return false;
            }
            break;

        case NODE_MUL:
            if (!node_compile(bytecode, parser, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, parser, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_MUL)) {
                return false;
            }
            break;

        case NODE_DIV:
            if (!node_compile(bytecode, parser, node->binary.left_index, stack_size)) {
                return false;
            }
            if (!node_compile(bytecode, parser, node->binary.right_index, result_stack_size)) {
                return false;
            }
            if (!bytecode_write_int(&bytecode->bytes, CODE_DIV)) {
                return false;
            }
            break;

        case NODE_INV:
            if (!node_compile(bytecode, parser, node->child_index, stack_size)) {
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
            if (!bytecode_write_size(&bytecode->bytes, node->name_offset + sizeof(size_t) * 2)) {
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

struct Bytecode bytecode_compile(const struct Parser *parser) {
    struct Bytecode bytecode = { .bytes = BUFFER_INIT, .stack_size = 0 };

    // ensure alignment to sizeof(long)
    size_t offset = parser->buffer.used + sizeof(size_t) * 2;
    size_t rem = offset % sizeof(long);
    if (rem > 0) {
        offset += sizeof(long) - rem;
    }

    // stack size placeholder
    if (!bytecode_write_size(&bytecode.bytes, 0)) {
        goto error;
    }

    // write offset to code start
    if (!bytecode_write_size(&bytecode.bytes, offset)) {
        goto error;
    }

    // write string table
    if (!buffer_append(&bytecode.bytes, parser->buffer.data, parser->buffer.used)) {
        goto error;
    }

    // alignment padding
    while (bytecode.bytes.used < offset) {
        if (!buffer_append_byte(&bytecode.bytes, 0)) {
            goto error;
        }
    }

    // generate bytecode
    if (!node_compile(&bytecode, parser, parser->nodes_used - 1, 0)) {
        goto error;
    }

    if (!bytecode_write_int(&bytecode.bytes, CODE_RET)) {
        goto error;
    }

    // fill in stack size
    memcpy(bytecode.bytes.data, &bytecode.stack_size, sizeof(bytecode.stack_size));

    goto end;

error:
    // TODO: better error handling
    bytecode.stack_size = 0;

end:

    return bytecode;
}

long bytecode_eval(const void *bytecode) {
    long *stack = malloc(sizeof(long) * *(const size_t*)bytecode);
    if (stack == NULL) {
        // TODO: error handling
        return 0;
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

    const void *codeptr = (const uint8_t*)bytecode + ((const size_t*)bytecode)[1];
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
    const char *name = bytecode + *(const size_t*)codeptr;
    const char *strvalue = getenv(name);
    long value = 0;
    if (strvalue == NULL) {
        fprintf(stderr, "WARNING: Environment variable %s is not set!\n", name);
    } else {
        char *endptr = NULL;
        value = strtol(strvalue, &endptr, 10);

        if (!*strvalue || *endptr) {
            fprintf(stderr, "WARNING: Error parsing environment variable %s=%s\n", name, strvalue);
        }
    }
    *stackptr = value;
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

void bytecode_print(const void *bytecode, FILE *stream) {
    const size_t stack_size = sizeof(long) * *(const size_t*)bytecode;
    const void *codeptr = (const uint8_t*)bytecode + ((const size_t*)bytecode)[1];

    fprintf(stdout, "stack size: %zu\n", stack_size / sizeof(long));
    
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
                const char *name = (const char*)bytecode + *(const size_t*)codeptr;
                codeptr += sizeof(size_t);
                fprintf(stream, "VAR %s\n", name);
                break;
            }

            case CODE_RET:
                fprintf(stream, "RET\n");
                return;
        }
    }
}