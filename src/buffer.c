#include "buffer.h"

#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

struct Buffer buffer_create(size_t initial_capacity) {
    struct Buffer buffer = { .data = NULL, .used = 0, .capacity = 0 };

    if (initial_capacity > 0) {
        buffer.data = malloc(initial_capacity);
        if (buffer.data) {
            buffer.capacity = initial_capacity;
        }
    }

    return buffer;
}

void buffer_clear(struct Buffer *buffer) {
    buffer->used = 0;
}

bool buffer_append(struct Buffer *buffer, const char *data, size_t size) {
    if (buffer->used > SIZE_MAX - size) {
        errno = ENOMEM;
        return false;
    }

    if (buffer->used + size > buffer->capacity) {
        if (buffer->capacity >= SIZE_MAX / 2) {
            errno = ENOMEM;
            return false;
        }
        const size_t new_capacity = buffer->capacity == 0 ?
            BUFSIZ :
            buffer->capacity * 2;

        char *new_data = realloc(buffer->data, new_capacity);

        if (new_data == NULL) {
            return false;
        }

        buffer->data     = new_data;
        buffer->capacity = new_capacity;
    }

    memcpy(buffer->data + buffer->used, data, size);

    buffer->used += size;

    return true;
}

bool buffer_append_byte(struct Buffer *buffer, char byte) {
    return buffer_append(buffer, &byte, 1);
}

void buffer_destroy(struct Buffer *buffer) {
    free(buffer->data);

    buffer->data = NULL;
    buffer->capacity = 0;
    buffer->used = 0;
}
