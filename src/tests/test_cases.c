#include "test.h"
#include "ast.h"
#include "parser.h"
#include "bytecode.h"
#include "optimizer.h"

TEST_OK_EXPR(const, "123", 123)

TEST_OK_EXPR(var, "foo", 123, TEST_ARG(foo, 123))

TEST_OK_EXPR(sign, "--+ -x", -123, TEST_ARG(x, 123))

TEST_OK_EXPR(x_plus_3, "x + 3", 126, TEST_ARG(x, 123))

TEST_OK_EXPR(x_minus_x, "x-x", 0, TEST_ARG(x, 123))

TEST_OK_EXPR(many_add_sub,
    "3 + (x - 5) - (3 - 0 + (1 - y) - -3) + 0 + (x + x) + 10 - z", 925,
    TEST_ARG(x, 123),
    TEST_ARG(y, 456),
    TEST_ARG(z, -99))

TEST_OK_EXPR(x_times_3, "x * 3", 369, TEST_ARG(x, 123))

TEST_OK_EXPR(many_mul,
    "5 * (x * 3 * -y) * (2 * z * (y * y) * 3) * 2", 1360800,
    TEST_ARG(x, 5),
    TEST_ARG(y, 6),
    TEST_ARG(z, -7))

TEST_OK_EXPR(times_0,
    "5 * (x * 3) * y * (z * 0)", 0,
    TEST_ARG(x, 5),
    TEST_ARG(y, 6),
    TEST_ARG(z, -7))

TEST_OK_EXPR(many_mul_div,
    "3 * y / x * (5 * z * 3 / (x * 2)) * (3 / 2) / x", -6,
    TEST_ARG(x, 5),
    TEST_ARG(y, 6),
    TEST_ARG(z, -7))

// TODO: more positive tests

TESTS_PARSER_ERROR(undef_var, "x", ERROR_UNDEFINED_VARIABLE, "y")

TESTS_PARSER_ERROR(illegal_arg_name, "0", ERROR_ILLEGAL_ARG_NAME, "foo bar")

TESTS_PARSER_ERROR(div_by_zero1, "1 / 0", ERROR_DIV_BY_ZERO)
TESTS_PARSER_ERROR(div_by_zero2, "x / 0", ERROR_DIV_BY_ZERO, "x")
TESTS_PARSER_ERROR(div_by_zero3, "(1 + x) / 0", ERROR_DIV_BY_ZERO, "x")

TESTS_PARSER_ERROR(dupli_arg_name, "0", ERROR_DUPLICATED_ARG_NAME, "x", "x")

// TODO: more illegal characters
TESTS_PARSER_ERROR(illegal_char, "x + $", ERROR_ILLEGAL_CHARACTER, "x")

// TODO: all kinds of illegal tokens
TESTS_PARSER_ERROR(illegal_token1, "x + * 2", ERROR_ILLEGAL_TOKEN, "x")
TESTS_PARSER_ERROR(illegal_token2, "x + (2))", ERROR_ILLEGAL_TOKEN, "x")

TESTS_PARSER_ERROR(expected_close1, "(x", ERROR_EXPECTED_CLOSE_PAREN, "x")
TESTS_PARSER_ERROR(expected_close2, "((x) ", ERROR_EXPECTED_CLOSE_PAREN, "x")
