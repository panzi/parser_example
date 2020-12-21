#include "ast.h"
#include "parser.h"
#include "optimizer.h"
#include "bytecode.h"
#include "tests/test.h"

#include <stdio.h>
#include <stdlib.h>

bool test_run(struct TestDecl const* const tests[]) {
    size_t count_success = 0;
    size_t count_failure = 0;
    size_t count = 0;

    printf("Running tests...\n\n");

    while (tests[count]) ++ count;

    struct TestResult *results = calloc(count, sizeof(struct TestResult));

    if (results == NULL) {
        perror("allocating test results array");
        return false;
    }

    for (size_t index = 0; index < count; ++ index) {
        const struct TestDecl *test = tests[index];
        printf("%s ... ", test->test_name);
        fflush(stdout);
        struct TestResult result = test->func();
        if (result.success) {
            puts("OK");
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
EXTERN_TEST(x_plus_3);
EXTERN_TEST(many_add_sub);

struct TestDecl const* const tests[] = {
    TEST_REF(const),
    TEST_REF(var),
    TEST_REF(x_plus_3),
    TEST_REF(many_add_sub),
    NULL
};

int main() {
    return test_run(tests) ? 0 : 1;
}
