#ifndef BYTECODE_H
#define BYTECODE_H
#pragma once

#include "buffer.h"
#include "parser.h"

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Bytecode {
    struct Buffer bytes;
    size_t stack_size;
};

struct Bytecode bytecode_compile(const struct Ast *ast);
void bytecode_destroy(struct Bytecode *bytecode);

long bytecode_eval(const void *bytecode, const long args[]);
void bytecode_print(const void *bytecode, char *const *const args, FILE *stream);

#ifdef __cplusplus
}
#endif

#endif
