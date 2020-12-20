#include "test.h"
#include "ast.h"
#include "parser.h"
#include "bytecode.h"
#include "optimizer.h"

TEST_DECL(add, {
    ASSERT_SUCCESS_EXPR("x + 3", 126l, { "x" }, { 123 })
})
