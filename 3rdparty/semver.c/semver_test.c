/*
 * semver_test.c
 *
 * Copyright (c) 2015-2017 Tomas Aparicio
 * MIT licensed
 */

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "semver.h"

#define test_start(x) \
  printf("\n# Test: %s\n", x)  \

#define test_end() \
  printf("OK\n")  \

struct test_case {
  char * x;
  char * y;
  int  expected;
};

struct test_case_match {
  char * x;
  char * y;
  char * op;
  int  expected;
};

typedef int (*fn)(semver_t, semver_t);

static void
compare_helper (char *a, char *b, int expected, fn test_fn) {
  semver_t verX = {0};
  semver_t verY = {0};

  semver_parse(a, &verX);
  semver_parse(b, &verY);

  int resolution = test_fn(verX, verY);
  assert(resolution == expected);

  semver_free(&verX);
  semver_free(&verY);
}

static void
suite_runner(struct test_case cases[], int len, fn test_fn) {
  int i;
  for (i = 0; i < len; i++) {
    struct test_case args = cases[i];
    compare_helper(args.x, args.y, args.expected, test_fn);
  }
}

void
test_parse_simple() {
  test_start("parse_simple");

  char buf[] = "1.2.12";
  semver_t ver;

  int error = semver_parse(buf, &ver);

  assert(error == 0);
  assert(ver.major == 1);
  assert(ver.minor == 2);
  assert(ver.patch == 12);

  semver_free(&ver);

  test_end();
}

void
test_parse_major() {
  test_start("parse_major");

  char buf[] = "2";
  semver_t ver = {0};

  int error = semver_parse(buf, &ver);

  assert(error == 0);
  assert(ver.major == 2);
  assert(ver.minor == 0);
  assert(ver.patch == 0);

  semver_free(&ver);

  test_end();
}

void
test_parse_minor() {
  test_start("parse_minor");

  char buf[] = "1.2";
  semver_t ver = {0};

  int error = semver_parse(buf, &ver);

  assert(error == 0);
  assert(ver.major == 1);
  assert(ver.minor == 2);
  assert(ver.patch == 0);

  semver_free(&ver);

  test_end();
}

void
test_parse_prerelease() {
  test_start("parse_prerelease");

  char buf[] = "1.2.12-beta.alpha.1.1";
  semver_t ver = {0};

  int error = semver_parse(buf, &ver);

  assert(error == 0);
  assert(ver.major == 1);
  assert(ver.minor == 2);
  assert(ver.patch == 12);
  assert(strcmp(ver.prerelease, "beta.alpha.1.1") == 0);

  semver_free(&ver);

  test_end();
}

void
test_parse_metadata() {
  test_start("parse_metadata");

  char buf[] = "1.2.12+20130313144700";
  semver_t ver = {0};

  int error = semver_parse(buf, &ver);

  assert(error == 0);
  assert(ver.major == 1);
  assert(ver.minor == 2);
  assert(ver.patch == 12);
  assert(strcmp(ver.metadata, "20130313144700") == 0);

  semver_free(&ver);

  test_end();
}

void
test_parse_prerelerease_metadata() {
  test_start("parse_prerelease_metadata");

  char buf[] = "1.2.12-alpha.1+20130313144700";
  semver_t ver = {0};

  int error = semver_parse(buf, &ver);

  assert(error == 0);
  assert(ver.major == 1);
  assert(ver.minor == 2);
  assert(ver.patch == 12);
  assert(strcmp(ver.prerelease, "alpha.1") == 0);
  assert(strcmp(ver.metadata, "20130313144700") == 0);

  semver_free(&ver);

  test_end();
}

void
test_compare() {
  test_start("semver_compare");

  struct test_case cases[] = {
    {"1", "0", 1},
    {"1", "1", 0},
    {"1", "3", -1},
    {"1.5", "0.8", 1},
    {"1.5", "1.3", 1},
    {"1.2", "2.2", -1},
    {"3.0", "1.5", 1},
    {"1.5", "1.5", 0},
    {"1.0.9", "1.0.0", 1},
    {"1.0.9", "1.0.9", 0},
    {"1.1.5", "1.1.9", -1},
    {"1.2.2", "1.1.9", 1},
    {"1.2.2", "1.2.9", -1},
  };

  suite_runner(cases, 13, &semver_compare);
  test_end();
}

void
test_compare_full() {
  test_start("semver_compare_full");

  struct test_case cases[] = {
    {"1.5.1", "1.5.1-beta", 1},
    {"1.5.1-beta", "1.5.1", -1},
    {"1.5.1-beta", "1.5.1-beta", 0},
    {"1.5.1-beta", "1.5.1-alpha", 1},
    {"1.5.1-beta.1", "1.5.1-alpha.1", 1},
    {"1.5.1-beta.1", "1.5.1-beta.0", 1},
    {"1.5.1-beta.1.5", "1.5.1-beta.1.5", 0},
    {"1.5.1-beta.1.5", "1.5.1-beta.1.4", 1},
    {"1.5.1-beta.1.0", "1.5.1-beta.1.4", -1},
    {"1.5.1-beta.1.0", "1.5.1-alpha.1.0", 1},
    {"1.5.1-beta.1.100", "1.5.1-alpha.1.99", 1},
    {"1.5.1-beta.1.123456789", "1.5.1-alpha.1.12345678", 1},
    {"1.5.1-beta.alpha.1", "1.5.1-beta.alpha.1.12345678", -1},
    {"1.5.1-beta.alpha.1", "1.5.1-beta.alpha.1+123", 0},
    {"1.5.1-beta.1+20130313144700", "1.5.1-beta.1+20120313144700", 0},
    {"1.5.1-beta.1+20130313144700", "1.5.1-beta.1+20130313144700", 0},
    {"1.5.1-beta.1+20130313144700", "1.5.1-beta.1+exp.sha.5114f85", 0},
    {"1.5.1-beta.1+exp.sha.5114f85", "1.5.1-beta.1+exp.sha.5114f84", 0},
    {"1.5.1-beta.1+exp.sha.5114f85", "1.5.1-beta.1+exp.sha1.5114f84", 0},
    {"1.5.1-beta.1+exp.sha", "1.5.1-beta.1+exp.sha256", 0},
    {"1.5.1-alpha.beta", "1.5.1-1.beta", 1},
  };

  suite_runner(cases, 21, &semver_compare);
  test_end();
}

void
test_compare_spec() {
  test_start("semver_compare_spec");

  /* from: http://semver.org/spec/v2.0.0.html#spec-item-11 */
  struct test_case cases[] = {
    {"1.0.0-alpha", "1.0.0-alpha.1", -1},
    {"1.0.0-alpha.1", "1.0.0-alpha.beta", -1},
    {"1.0.0-alpha.beta", "1.0.0-beta", -1},
    {"1.0.0-beta", "1.0.0-beta.2", -1},
    {"1.0.0-beta.2", "1.0.0-beta.11", -1},
    {"1.0.0-beta.11", "1.0.0-rc.1", -1},
    {"1.0.0-rc.1", "1.0.0", -1},
  };

  suite_runner(cases, 7, &semver_compare);
  test_end();
}

void
test_compare_gt() {
  test_start("semver_gt");

  struct test_case cases[] = {
    {"1", "0", 1},
    {"1", "3", 0},
    {"3.0", "1", 1},
    {"1.0", "3", 0},
    {"1.0", "1", 0},
    {"1.5", "0.8", 1},
    {"1.2", "2.2", 0},
    {"3.0", "1.5", 1},
    {"1.1", "1.0.9", 1},
    {"1.0", "1.0.0", 0},
    {"1.1.9", "1.2", 0},
    {"1.0.9", "1.0.0", 1},
    {"1.0.9", "1.0.9", 0},
    {"1.1.5", "1.1.9", 0},
    {"1.2.2", "1.2.9", 0},
  };

  suite_runner(cases, 15, &semver_gt);
  test_end();
}

void
test_compare_lt() {
  test_start("semver_lt");

  struct test_case cases[] = {
    {"1", "0", 0},
    {"1", "3", 1},
    {"1.5", "0.8", 0},
    {"1.2", "2.2", 1},
    {"3.0", "1.5", 0},
    {"1.0.9", "1.0.0", 0},
    {"1.0.9", "1.0.9", 0},
    {"1.1.5", "1.1.9", 1},
    {"1.2.2", "1.2.9", 1},
  };

  suite_runner(cases, 9, &semver_lt);
  test_end();
}

void
test_compare_eq() {
  test_start("semver_eq");

  struct test_case cases[] = {
    {"1", "0", 0},
    {"1", "3", 0},
    {"1", "1", 1},
    {"1.5", "0.8", 0},
    {"1.2", "2.2", 0},
    {"3.0", "1.5", 0},
    {"1.0", "1.0", 1},
    {"1.0.9", "1.0.0", 0},
    {"1.1.5", "1.1.9", 0},
    {"1.2.2", "1.2.9", 0},
    {"1.0.0", "1.0.0", 1},
  };

  suite_runner(cases, 11, &semver_eq);
  test_end();
}

void
test_compare_neq() {
  test_start("semver_neq");

  struct test_case cases[] = {
    {"1", "0", 1},
    {"1", "3", 1},
    {"1", "1", 0},
    {"1.5", "0.8", 1},
    {"1.2", "2.2", 1},
    {"3.0", "1.5", 1},
    {"1.0", "1.0", 0},
    {"1.0.9", "1.0.0", 1},
    {"1.1.5", "1.1.9", 1},
    {"1.2.2", "1.2.9", 1},
    {"1.0.0", "1.0.0", 0},
  };

  suite_runner(cases, 11, &semver_neq);
  test_end();
}

void
test_compare_gte() {
  test_start("semver_gte");

  struct test_case cases[] = {
    {"1", "0", 1},
    {"1", "3", 0},
    {"1", "1", 1},
    {"1.5", "0.8", 1},
    {"1.2", "2.2", 0},
    {"3.0", "1.5", 1},
    {"1.0", "1.0", 1},
    {"1.0.9", "1.0.0", 1},
    {"1.1.5", "1.1.9", 0},
    {"1.2.2", "1.2.9", 0},
    {"1.0.0", "1.0.0", 1},
  };

  suite_runner(cases, 11, &semver_gte);
  test_end();
}

void
test_compare_lte() {
  test_start("semver_lte");

  struct test_case cases[] = {
    {"1", "0", 0},
    {"1", "3", 1},
    {"1", "1", 1},
    {"1.5", "0.8", 0},
    {"1.2", "2.2", 1},
    {"3.0", "1.5", 0},
    {"1.0", "1.0", 1},
    {"1.0.9", "1.0.0", 0},
    {"1.1.5", "1.1.9", 1},
    {"1.2.2", "1.2.9", 1},
    {"1.0.0", "1.0.0", 1},
  };

  suite_runner(cases, 11, &semver_lte);
  test_end();
}

void
test_satisfies() {
  test_start("semver_satisfies");

  struct test_case_match cases[] = {
    {"1", "0", ">=", 1},
    {"1", "3", ">=", 0},
    {"1", "1", ">=", 1},
    {"1.5", "0.8", ">=", 1},
    {"1.2", "2.2", ">=", 0},
    {"3.0", "1.5", ">=", 1},
    {"1.0", "1.0", ">=", 1},
    {"1.0.9", "1.0.0", ">=", 1},
    {"1.1.5", "1.1.9", ">=", 0},
    {"1.2.2", "1.2.9", ">=", 0},
    {"1.0.0", "1.0.0", ">=", 1},

    {"1", "0", "<=", 0},
    {"1", "3", "<=", 1},
    {"1", "1", "<=", 1},
    {"1.5", "0.8", "<=", 0},
    {"1.2", "2.2", "<=", 1},
    {"3.0", "1.5", "<=", 0},
    {"1.0", "1.0", "<=", 1},
    {"1.0.9", "1.0.0", "<=", 0},
    {"1.1.5", "1.1.9", "<=", 1},
    {"1.2.2", "1.2.9", "<=", 1},
    {"1.0.0", "1.0.0", "<=", 1},

    {"1", "0", "=", 0},
    {"1", "3", "=", 0},
    {"1", "1", "=", 1},
    {"1.5", "0.8", "=", 0},
    {"1.2", "2.2", "=", 0},
    {"3.0", "1.5", "=", 0},
    {"1.0", "1.0", "=", 1},
    {"1.0.9", "1.0.0", "=", 0},
    {"1.1.5", "1.1.9", "=", 0},
    {"1.2.2", "1.2.9", "=", 0},
    {"1.0.0", "1.0.0", "=", 1},

    {"1", "0", ">", 1},
    {"1", "3", ">", 0},
    {"1", "1", ">", 0},
    {"1.5", "0.8", ">", 1},
    {"1.2", "2.2", ">", 0},
    {"3.0", "1.5", ">", 1},
    {"1.0", "1.0", ">", 0},
    {"1.0.9", "1.0.0", ">", 1},
    {"1.1.5", "1.1.9", ">", 0},
    {"1.2.2", "1.2.9", ">", 0},
    {"1.0.0", "1.0.0", ">", 0},

    {"1", "0", "<", 0},
    {"1", "3", "<", 1},
    {"1", "1", "<", 0},
    {"1.5", "0.8", "<", 0},
    {"1.2", "2.2", "<", 1},
    {"3.0", "1.5", "<", 0},
    {"1.0", "1.0", "<", 0},
    {"1.0.9", "1.0.0", "<", 0},
    {"1.1.5", "1.1.9", "<", 1},
    {"1.2.2", "1.2.9", "<", 1},
    {"1.0.0", "1.0.0", "<", 0},

    {"1", "0", "^", 0},
    {"1", "3", "^", 0},
    {"1", "1", "^", 1},
    {"1.5", "0.8", "^", 0},
    {"1.2", "2.2", "^", 0},
    {"3.0", "1.5", "^", 0},
    {"1.0", "1.0", "^", 1},
    {"1.0.9", "1.0.0", "^", 1},
    {"1.1.5", "1.1.9", "^", 1},
    {"1.3.2", "1.1.9", "^", 1},
    {"1.1.2", "1.5.9", "^", 1},
    {"0.1.2", "1.5.9", "^", 0},
    {"0.1.2", "0.2.9", "^", 0},
    {"1.2.2", "1.2.9", "^", 1},
    {"1.0.0", "1.0.0", "^", 1},

    {"1", "0", "~", 0},
    {"1", "3", "~", 0},
    {"1", "1", "~", 1},
    {"1.5", "0.8", "~", 0},
    {"1.2", "2.2", "~", 0},
    {"3.0", "1.5", "~", 0},
    {"1.0", "1.0", "~", 1},
    {"1.0.9", "1.0.0", "~", 1},
    {"1.1.5", "1.1.9", "~", 1},
    {"1.1.9", "1.1.3", "~", 1},
    {"1.2.2", "1.3.9", "~", 0},
    {"1.0.0", "1.0.0", "~", 1},
  };

  int i;
  for (i = 0; i < 82; i++) {
    struct test_case_match args = cases[i];

    semver_t verX = {0};
    semver_t verY = {0};

    semver_parse(args.x, &verX);
    semver_parse(args.y, &verY);

    int resolution = semver_satisfies(verX, verY, args.op);
    assert(resolution == args.expected);


    semver_free(&verX);
    semver_free(&verY);
  }

  test_end();
}

/**
 * Renders
 */

void
test_render() {
  test_start("render");

  semver_t ver = {1, 5, 8, NULL, NULL};
  char * str[10] = {0};
  semver_render(&ver, (char *) str);
  assert(strcmp((char *) str, "1.5.8") == 0);

  semver_t ver2 = {1, 5, 8, NULL, NULL};
  ver2.prerelease = "alpha.1";
  ver2.metadata = "1232323";
  char * str2[22] = {0};
  semver_render(&ver2, (char *) str2);
  assert(strcmp((char *) str2, "1.5.8-alpha.1+1232323") == 0);

  test_end();
}

void
test_numeric() {
  test_start("numeric");

  semver_t v0 = {0, 5, 9, NULL, NULL};
  assert(semver_numeric(&v0) == 59);

  semver_t v1 = {1, 3, 6, NULL, NULL};
  assert(semver_numeric(&v1) == 136);

  semver_t v2 = {1, 3, 6, "beta", "123456789"};
  assert(semver_numeric(&v2) == 1025);

  semver_t v3 = {1, 3, 6, "be$ta", "12&345@67(89"};
  assert(semver_numeric(&v3) == 1025);

  test_end();
}

/**
 * Modifiers
 */

void
test_bump() {
  test_start("bump");

  semver_t ver = {1, 5, 8, NULL, NULL};
  semver_bump(&ver);
  assert(ver.major == 2);
  semver_free(&ver);

  semver_t ver2 = {1, 0, 0, NULL, NULL};
  semver_bump(&ver2);
  assert(ver2.major == 2);
  semver_free(&ver2);

  semver_t ver3 = {0, 0, 0, NULL, NULL};
  semver_bump(&ver3);
  assert(ver3.major == 1);
  semver_free(&ver3);

  test_end();
}

void
test_bump_minor() {
  test_start("bump_minor");

  semver_t ver = {1, 5, 8, NULL, NULL};
  semver_bump_minor(&ver);
  assert(ver.minor == 6);
  semver_free(&ver);

  semver_t ver2 = {1, 0, 0, NULL, NULL};
  semver_bump_minor(&ver2);
  assert(ver2.minor == 1);
  semver_free(&ver2);

  test_end();
}

void
test_bump_patch() {
  test_start("bump_patch");

  semver_t ver = {1, 5, 8, NULL, NULL};
  semver_bump_patch(&ver);
  assert(ver.patch == 9);
  semver_free(&ver);

  semver_t ver2 = {1, 5, 0, NULL, NULL};
  semver_bump_patch(&ver2);
  assert(ver2.patch == 1);
  semver_free(&ver2);

  test_end();
}

/**
 * Clean up
 */

void
test_free () {
  test_start("free");

  semver_t ver = {0};
  char * str = "1.5.6";
  semver_parse(str, &ver);
  assert(ver.major == 1);
  assert(ver.patch == 6);
  semver_free(&ver);

  semver_t ver2 = {0};
  char * str2 = "1.5.6-beta.1+12345";
  semver_parse(str2, &ver2);
  assert(ver2.prerelease != NULL);
  semver_free(&ver2);
  assert(ver2.prerelease == NULL);

  test_end();
}

/**
 * Helper functions
 */

void
test_valid_chars() {
  test_start("valid_chars");

  int valid;

  valid = semver_is_valid("1");
  assert(valid == 1);

  valid = semver_is_valid("159");
  assert(valid == 1);

  valid = semver_is_valid("1b3");
  assert(valid == 1);

  valid = semver_is_valid("3 0 1");
  assert(valid == 0);

  valid = semver_is_valid("&3@(");
  assert(valid == 0);

  test_end();
}

void
test_clean() {
  test_start("clean");

  int error;

  char str[] = "1.2.3";
  error = semver_clean(str);
  assert(strcmp(str, "1.2.3") == 0);
  assert(error == 0);

  char str2[] = " 1.@2.3-beta #.alpha+12@34  ";
  error = semver_clean(str2);
  assert(strcmp(str2, "1.2.3-beta.alpha+1234") == 0);
  assert(error == 0);

  test_end();
}

int
main() {

  /* Parser */
  test_parse_simple();
  test_parse_major();
  test_parse_minor();
  test_parse_prerelease();
  test_parse_metadata();
  test_parse_prerelerease_metadata();

  /* Comparison */
  test_compare();
  test_compare_full();
  test_compare_spec();
  test_compare_gt();
  test_compare_lt();
  test_compare_eq();
  test_compare_neq();
  test_compare_gte();
  test_compare_lte();
  test_satisfies();

  /* Renders */
  test_render();
  test_numeric();

  /* Modifiers */
  test_bump();
  test_bump_minor();
  test_bump_patch();

  /* Clean up */
  test_free();

  /* Helpers */
  test_valid_chars();
  test_clean();

  return 0;
}
