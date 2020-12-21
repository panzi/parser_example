#include "test.h"
#include "ast.h"
#include "parser.h"
#include "bytecode.h"
#include "optimizer.h"

#define LIST(...) {__VA_ARGS__}

TEST_SUCCESS_EXPR(const, "123", 123)

TEST_SUCCESS_EXPR(var, "foo", 123, TEST_ARG(foo, 123))

TEST_SUCCESS_EXPR(x_plus_3, "x + 3", 126, TEST_ARG(x, 123))

TEST_SUCCESS_EXPR(many_add_sub,
    "3 + (x - 5) - (3 + (1 - y) - -3) + (x + x) + 10", 826,
    TEST_ARG(x, 123),
    TEST_ARG(y, 456))

