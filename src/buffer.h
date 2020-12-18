#ifndef BUFFER_H
#define BUFFER_H
#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Buffer {
    char *data;
    size_t used;
    size_t capacity;
};

#define BUFFER_INIT (struct Buffer){ .data = NULL, .used = 0, .capacity = 0 }

struct Buffer buffer_create(size_t initial_capacity);
void buffer_clear(struct Buffer *buffer);
bool buffer_append(struct Buffer *buffer, const char *data, size_t size);
bool buffer_append_byte(struct Buffer *buffer, char byte);
void buffer_destroy(struct Buffer *buffer);

#ifdef __cplusplus
}
#endif

#endif
