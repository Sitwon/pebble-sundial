#ifndef SUNDIAL_TEST_H
#define SUNDIAL_TEST_H

#include <stdio.h>

static int g_failures = 0;

#define CHECK(cond) do { \
  if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    g_failures++; \
  } \
} while (0)

#define CHECK_NEAR(a, b, tol) CHECK((a) >= (b) - (tol) && (a) <= (b) + (tol))

static int test_summary(const char *name) {
  if (g_failures) {
    printf("%s: %d failure(s)\n", name, g_failures);
    return 1;
  }
  printf("%s: ok\n", name);
  return 0;
}

#endif
