#include "ast.h"
#include "parser.h"
#include "optimizer.h"
#include "bytecode.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool test_run(struct TestDecl const* const tests[]) {
    size_t count_success = 0;
    size_t count_failure = 0;
    size_t count = 0;
    size_t max_name_len = 0;

    printf("Running tests...\n\n");

    while (tests[count]) {
        const size_t name_len = strlen(tests[count]->test_name);
        if (max_name_len < name_len) {
            max_name_len = name_len;
        }
        ++ count;
    }

    struct TestResult *results = calloc(count, sizeof(struct TestResult));

    if (results == NULL) {
        perror("allocating test results array");
        return false;
    }

    for (size_t index = 0; index < count; ++ index) {
        const struct TestDecl *test = tests[index];
        size_t dots = max_name_len - strlen(test->test_name) + 3;

        printf("%s ", test->test_name);
        while (dots --) {
            putchar('.');
        }
        putchar(' ');
        fflush(stdout);

        struct TestResult result = test->func();
        if (result.success) {
            puts(" OK ");
            ++ count_success;
        } else {
            puts("FAIL");
            ++ count_failure;
        }
        results[index] = result;
    }

    if (count_failure > 0) {
        printf("\nTest Failures:\n");

        for (size_t index = 0; index < count; ++ index) {
            const struct TestResult *result = &results[index];
            if (!result->success) {
                const struct TestDecl *test = tests[index];
                printf(" * %s:\n", test->test_name);
                printf("    %s:%zu: %s\n", result->file_name, result->lineno, result->message);
                printf("    %s\n\n", result->expr);
            }
        }
    }

    for (size_t index = 0; index < count; ++ index) {
        free(results[index].message);
    }

    free(results);

    printf("\nTests: %zu, Successes: %zu, Failures: %zu\n",
        count, count_success, count_failure);

    return count_failure == 0;
}

// TODO: more tests

EXTERN_TEST(const);
EXTERN_TEST(var);
EXTERN_TEST(sign);
EXTERN_TEST(x_plus_3);
EXTERN_TEST(x_minus_x);
EXTERN_TEST(many_add_sub);
EXTERN_TEST(x_times_3);
EXTERN_TEST(many_mul);
EXTERN_TEST(times_0);
EXTERN_TEST(many_mul_div);
EXTERN_TEST(undef_var);
EXTERN_TEST(illegal_arg_name);
EXTERN_TEST(div_by_zero1);
EXTERN_TEST(div_by_zero2);
EXTERN_TEST(div_by_zero3);
EXTERN_TEST(dupli_arg_name);
EXTERN_TEST(illegal_char);
EXTERN_TEST(illegal_token1);
EXTERN_TEST(illegal_token2);
EXTERN_TEST(expected_close1);
EXTERN_TEST(expected_close2);

struct TestDecl const* const tests[] = {
    TEST_REF(const),
    TEST_REF(var),
    TEST_REF(sign),
    TEST_REF(x_plus_3),
    TEST_REF(x_minus_x),
    TEST_REF(many_add_sub),
    TEST_REF(x_times_3),
    TEST_REF(many_mul),
    TEST_REF(times_0),
    TEST_REF(many_mul_div),
    TEST_REF(undef_var),
    TEST_REF(illegal_arg_name),
    TEST_REF(div_by_zero1),
    TEST_REF(div_by_zero2),
    TEST_REF(div_by_zero3),
    TEST_REF(dupli_arg_name),
    TEST_REF(illegal_char),
    TEST_REF(illegal_token1),
    TEST_REF(illegal_token2),
    TEST_REF(expected_close1),
    TEST_REF(expected_close2),
    NULL
};

int main() {
    return test_run(tests) ? 0 : 1;
}
