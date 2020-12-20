#ifndef TEST_H
#define TEST_H
#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

struct TestResult {
    bool success;
    const char *file_name;
    size_t lineno;
    const char *expr;
    char *message;
};

struct TestDecl {
    const char *test_name;
    const char *file_name;
    size_t lineno;
    struct TestResult (*func)();
};

#define TEST_STR(EXPR) #EXPR

#define TEST_DECL(NAME, BODY) \
    struct TestResult test_func_ ## NAME () { \
        struct TestResult _test_result = (struct TestResult) { \
            .success   = true, \
            .file_name = __FILE__, \
            .lineno    = __LINE__, \
            .expr      = NULL, \
            .message   = NULL, \
        }; \
        BODY; \
        return _test_result; \
    } \
    const struct TestDecl test_decl_ ## NAME = { \
        .test_name = TEST_STR(NAME), \
        .file_name = __FILE__, \
        .lineno    = __LINE__, \
        .func      = &test_func_ ## NAME, \
    };

#define TEST_REF(NAME) &(test_decl_ ## NAME)
#define TEST_FWD(NAME) extern const struct TestDecl test_decl_ ## NAME;

#define ASSERT_WITH_MESSAGE(EXPR, MSG, ...) \
    if (!(EXPR)) { \
        int _test_msg_len = snprintf(NULL, 0, MSG __VA_OPT__(,) __VA_ARGS__); \
        char *_test_msg = NULL; \
        if (_test_msg_len < 0) { \
            perror("printing message: " MSG); \
        } else { \
            _test_msg = calloc(_test_msg_len + 1, 1); \
            if (_test_msg) { \
                snprintf(_test_msg, _test_msg_len + 1, MSG __VA_OPT__(,) __VA_ARGS__); \
            } \
        } \
        _test_result = (struct TestResult) { \
            .success   = false, \
            .file_name = __FILE__, \
            .lineno    = __LINE__, \
            .expr      = TEST_STR(EXPR), \
            .message   = _test_msg, \
        }; \
        goto cleanup; \
    }

#define _ASSERT_WITH_OPT_MESSAGE(EXPR, MSG, ...) \
    ASSERT_WITH_MESSAGE(EXPR, MSG, __VA_ARGS__)

#define ASSERT_TRUE(EXPR, MSG, ...) \
    ASSERT_WITH_MESSAGE(EXPR, MSG, __VA_ARGS__)

#define ASSERT_EQUAL(EXPECTED, ACTUAL, MSG, ...) \
    ASSERT_WITH_MESSAGE(EXPECTED == ACTUAL, MSG, __VA_ARGS__)

#define ASSERT_NOT_EQUAL(EXPECTED, ACTUAL, MSG, ...) \
    ASSERT_WITH_MESSAGE(EXPECTED != ACTUAL, MSG, __VA_ARGS__)

#define ASSERT_SUCCESS_EXPR(EXPR, RESULT, ARG_NAMES, ARG_VALUES) \
    struct Parser parser = PARSER_INIT; \
    struct Bytecode bytecode = BYTECODE_INIT; \
    \
    char *arg_names[] = ARG_NAMES; \
    const long arg_values[] = ARG_VALUES; \
    \
    parser = parse_string((EXPR), arg_names, sizeof(arg_names) / sizeof(arg_names[0])); \
    ASSERT_EQUAL(ERROR_NONE, parser.error, "parsing failed"); \
    \
    const long ast_result = ast_eval(&parser.ast, arg_values); \
    ASSERT_EQUAL(RESULT, ast_result, "AST interpretation failed: %ld != %ld", (RESULT), ast_result); \
    \
    optimize(&parser.ast); \
    \
    const long opt_result = ast_eval(&parser.ast, arg_values); \
    ASSERT_EQUAL(RESULT, opt_result, "optimized AST interpretation failed: %ld != %ld", (RESULT), opt_result); \
    \
    bytecode = bytecode_compile(&parser.ast); \
    ASSERT_NOT_EQUAL(0, bytecode.stack_size, "bytecode compilation failed"); \
    \
    const long bytecode_result = bytecode_eval(bytecode.bytes.data, arg_values); \
    ASSERT_EQUAL(RESULT, bytecode_result, "bytecode interpretation failed: %ld != %ld", (RESULT), bytecode_result); \
    \
cleanup: \
    parser_destroy(&parser); \
    bytecode_destroy(&bytecode);

bool test_run(struct TestDecl const* const tests[]);

#ifdef __cplusplus
}
#endif

#endif
