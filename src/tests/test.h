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

#define TEST_DECL_SYM(SYM, NAME) \
    static void test_func_inner_ ## SYM (struct TestResult *_test_result); \
    static struct TestResult test_func_wrapper_ ## SYM (); \
    \
    const struct TestDecl test_decl_ ## SYM = { \
        .test_name = NAME, \
        .file_name = __FILE__, \
        .lineno    = __LINE__, \
        .func      = &test_func_wrapper_ ## SYM, \
    }; \
    \
    struct TestResult test_func_wrapper_ ## SYM () { \
        struct TestResult _test_result = { \
            .success   = true, \
            .file_name = __FILE__, \
            .lineno    = __LINE__, \
            .expr      = NULL, \
            .message   = NULL, \
        }; \
        test_func_inner_ ## SYM (&_test_result); \
        return _test_result; \
    } \
    \
    void test_func_inner_ ## SYM (struct TestResult *_test_result)

#define TEST_DECL(SYM) TEST_DECL_SYM(SYM, TEST_STR(SYM))

#define TEST_REF(NAME) &(test_decl_ ## NAME)
#define EXTERN_TEST(NAME) extern const struct TestDecl test_decl_ ## NAME;

struct TestArg {
    const char *name;
    long value;
};

#define TEST_ARG(NAME, VALUE) \
    (struct TestArg) { .name = TEST_STR(NAME), .value = (VALUE) }

#define ASSERT_WITH_MESSAGE(EXPR, MSG, ...) \
    if (!(EXPR)) { \
        int _test_msg_len = snprintf(NULL, 0, MSG __VA_OPT__(,) __VA_ARGS__); \
        char *_test_msg = NULL; \
        if (_test_msg_len < 0) { \
            perror("printing message: " MSG " for expression: " TEST_STR(EXPR)); \
        } else { \
            _test_msg = calloc(_test_msg_len + 1, 1); \
            if (_test_msg) { \
                snprintf(_test_msg, _test_msg_len + 1, MSG __VA_OPT__(,) __VA_ARGS__); \
            } \
        } \
        *_test_result = (struct TestResult) { \
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

#define ASSERT_SUCCESS_EXPR(EXPR, RESULT, ...) \
    const struct TestArg test_args[] = { __VA_ARGS__ }; \
    (void)test_args; \
    const char *arg_names[sizeof(test_args) / sizeof(struct TestArg)]; \
    long arg_values[sizeof(test_args) / sizeof(struct TestArg)]; \
    size_t size = sizeof(test_args) / sizeof(struct TestArg); \
    for (size_t index = 0; index < size; ++ index) { \
        arg_names[index]  = test_args[index].name; \
        arg_values[index] = test_args[index].value; \
    } \
    \
    parser = parse_string((EXPR), (char *const *const)arg_names, sizeof(test_args) / sizeof(struct TestArg)); \
    ASSERT_EQUAL(ERROR_NONE, parser.error, "parsing failed"); \
    \
    const long ast_result = ast_eval(&parser.ast, arg_values); \
    ASSERT_EQUAL(RESULT, ast_result, "AST interpretation failed: %ld != %ld", (long)(RESULT), ast_result); \
    \
    optimize(&parser.ast); \
    \
    const long opt_result = ast_eval(&parser.ast, arg_values); \
    ASSERT_EQUAL(RESULT, opt_result, "optimized AST interpretation failed: %ld != %ld", (long)(RESULT), opt_result); \
    \
    bytecode = bytecode_compile(&parser.ast); \
    ASSERT_NOT_EQUAL(0, bytecode.stack_size, "bytecode compilation failed"); \
    \
    const long bytecode_result = bytecode_eval(bytecode.bytes.data, arg_values); \
    ASSERT_EQUAL(RESULT, bytecode_result, "bytecode interpretation failed: %ld != %ld", (long)(RESULT), bytecode_result);

#define TEST_SUCCESS_EXPR(NAME, EXPR, RESULT, ...) \
    TEST_DECL_SYM(NAME, EXPR " == " TEST_STR(RESULT)) { \
        struct Parser parser = PARSER_INIT; \
        struct Bytecode bytecode = BYTECODE_INIT; \
        \
        ASSERT_SUCCESS_EXPR(EXPR, RESULT, __VA_ARGS__); \
        \
    cleanup: \
        parser_destroy(&parser); \
        bytecode_destroy(&bytecode); \
    }


bool test_run(struct TestDecl const* const tests[]);

#ifdef __cplusplus
}
#endif

#endif
