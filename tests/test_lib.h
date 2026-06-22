#ifndef TEST_LIB_H
#define TEST_LIB_H

#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <setjmp.h>

#ifdef TEST_COLOR_OUTPUT
#define TEST_TERM_RED_FG "\x1b[31m"
#define TEST_TERM_GREEN_FG "\x1b[32m"
#define TEST_TERM_CYAN_FG "\x1b[36m"
#define TEST_TERM_RESET "\x1b[0m"
#else
#define TEST_TERM_RED_FG
#define TEST_TERM_GREEN_FG
#define TEST_TERM_CYAN_FG
#define TEST_TERM_RESET
#endif

#define test_assert(cond) \
do { \
    if (!(cond)) { \
        test__case_failed(__FILE__, __LINE__, "%s == false", #cond); \
    } \
} while(0)

#define define_test_assert_eq(T, left, right, fmt) \
do { \
    T _left = (left); \
    T _right = (right); \
    if (_left != _right) { \
        test__case_failed(__FILE__, __LINE__, fmt, _left, _right); \
    } \
} while(0)

#define test_assert_eq_int(T, left, right) define_test_assert_eq(T, left, right, "%d != %d")
#define test_assert_eq_uint(T, left, right) define_test_assert_eq(T, left, right, "%u != %u")
#define test_assert_eq_size_t(left, right) define_test_assert_eq(size_t, left, right, "%zu != %zu")
#define test_assert_eq_ptrdiff_t(left, right) define_test_assert_eq(ptrdiff_t, left, right, "%td != %td")
#define test_assert_eq_char(left, right) define_test_assert_eq(char, left, right, "'%c' != '%c'")
#define test_assert_eq_enum(left, right) \
do { \
    int _left = (left); \
    int _right = (right); \
    if (_left != _right) { \
        test__case_failed(__FILE__, __LINE__, "expected: %s. got: %s == %d.", #right, #left, _left); \
    } \
} while(0)

#define test_case(...) for (\
    int _i = (test__case_set_name(__VA_ARGS__), test__success = true, test__case_jmp_result = setjmp(test__case_jmp), 0); \
    _i < 1 && test__case_jmp_result == 0; \
    _i++, test__successful_count++, test__case_result())

static char test__name[128];
static bool test__success = true;
static const char * test__category = NULL;
static int test__successful_count = 0;
static int test__failed_count = 0;
static jmp_buf test__case_jmp = {0};
static int test__case_jmp_result = 0;

void test__case_set_name(const char *fmt, ...)
{
    va_list list;
    va_start(list, fmt);
    int written = vsnprintf(test__name, sizeof(test__name), fmt, list);
    va_end(list);

    if ((unsigned long) written >= sizeof(test__name)) {
        fprintf(stderr, TEST_TERM_RED_FG"ERROR: test case name was too large"TEST_TERM_RESET"\n");
        exit(1);
    }
}

void test__case_result(void)
{
    const char *status = test__success
        ? TEST_TERM_GREEN_FG"OK  "TEST_TERM_RESET
        : TEST_TERM_RED_FG"FAIL"TEST_TERM_RESET;

    printf("[%s] %-45s "TEST_TERM_CYAN_FG"(%s)"TEST_TERM_RESET"\n", status, test__name, test__category);
}

void test__case_failed(const char *file, int line, const char *fmt, ...)
{
    test__success = false;
    test__failed_count++;
    test__case_result();

    printf(TEST_TERM_RED_FG"       assertion failed on %s:%d: ", file, line);

    va_list list;
    va_start(list, fmt);
    vprintf(fmt, list);
    va_end(list);

    printf(TEST_TERM_RESET"\n");

    longjmp(test__case_jmp, 1);
}

void test_category(const char *test_category)
{
    test__category = test_category;
    printf(TEST_TERM_CYAN_FG"----- %s -----"TEST_TERM_RESET"\n", test_category); \
}

int test_summary(void) {
#ifdef TEST_COLOR_OUTPUT
    if (test__failed_count > 0) {
        printf(TEST_TERM_RED_FG);
    } else {
        printf(TEST_TERM_GREEN_FG);
    }
#endif

    printf("Result: %3d Total %3d Passed %3d Failed\n",
        test__successful_count + test__failed_count,
        test__successful_count,
        test__failed_count);

#ifdef TEST_COLOR_OUTPUT
    printf(TEST_TERM_RESET);
    fflush(stdout);
#endif

    return test__failed_count > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

#endif // TEST_LIB_H
