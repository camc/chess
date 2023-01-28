// Used for unit testing in /src/tests/* when CHESS_TEST is defined

#include <assert.h>
#include <stdio.h>

// A test function runs a test and returns an int,
// 0 indicating the test passed and -1 indicating it failed
typedef int (*TestFunction)(void);

// Defines a new testing module (`mod`) containing the test functions in the
// following arguments (must be at least 1)
#define DEF_TEST_MODULE(mod, ...) static TestFunction _test_##mod##__TESTS[] = {__VA_ARGS__, NULL}

// Runs all tests in a module (`mod`), gets the list of tests from the variables
// set by DEF_TEST_MODULE
#define RUN_TEST_MODULE(mod)                         \
    do {                                             \
        puts("Running tests in `" #mod "`");         \
        _test_run_test_module(_test_##mod##__TESTS); \
    } while (0)

// Used by tests to assert that `cond` must be true.
// If it is not true, the test fails
#define TEST_ASSERT(cond, msg)                            \
    if (!(cond)) {                                        \
        printf("\t[x] %s failed: %s\n", __func__, (msg)); \
        return -1;                                        \
    }

// Helper function used in RUN_TEST_MODULE
static void _test_run_test_module(TestFunction *tests) {
    int passed = 0;
    int i;
    for (i = 0; tests[i] != NULL; i++) {
        passed += tests[i]() == 0;
    }

    printf("\t%d/%d tests passed\n", passed, i);
}

#include "test_engine.c"
#include "test_fen.c"

void test_main() {
    RUN_TEST_MODULE(engine);
    RUN_TEST_MODULE(fen);
}
