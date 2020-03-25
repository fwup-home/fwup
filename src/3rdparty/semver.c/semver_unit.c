#include <assert.h>

#include "semver.c"

void test_strcut_first(char *in, int offset, int len, char *expected) {
    char *p;
    /* Setup */
    p = calloc(strlen(in) + 1, sizeof(*in));
    memcpy(p, in, strlen(in) + 1);

    strcut(p, offset, len);

    /*printf("%d, %d, %s\n", len, offset, p);*/
    assert(memcmp(p, expected, strlen(expected) + 1) == 0);
}

void test_strcut_second(char *in, char sep, int side, char *expected) {
    char *p;
    int offset;
    /* Setup */
    p = calloc(strlen(in) + 1, sizeof(*in));
    memcpy(p, in, strlen(in) + 1);
    offset = strchr(p, sep) - p;

    if(side) {
        strcut(p, offset, -1);
    } else {
        strcut(p, 0, offset + 1);
    }

    /*printf("%d, %d, %s\n", len, offset, p);*/
    assert(memcmp(p, expected, strlen(expected) + 1) == 0);
}

void test_strcut() {
    int i, n;
    struct strcut_test_case {
        char *in;
        int offset;
        int len;
        char *expected;
    };

    struct strcut_test_case cases[] = {
        {"ALPHA.BETA", 0, 0, "ALPHA.BETA"},
        {"ALPHA.BETA", 0, 1, "LPHA.BETA"},
        {"ALPHA.BETA", 9, 1, "ALPHA.BET"},
        {"ALPHA.BETA", 5, 100, "ALPHA"},
        {"ALPHA.BETA", 5, -1, "ALPHA"},
        {"ALPHA.BETA", 0, 6, "BETA"},
        {"ALPHA.BETA", 0, 6, "BETA"},
        {"ALPHA.BETA", 2, 6, "ALTA"},
    };

    printf("TEST 1: ");

    n = sizeof(cases) / sizeof(struct strcut_test_case);
    for(i = 0; i < n; ++i) {
        test_strcut_first(cases[i].in, cases[i].offset, cases[i].len, cases[i].expected);
    }

    printf("OK\n");

    struct strcut_test_case_2 {
        char *in;
        char sep;
        int side; /* 0 = remove left side, 1 = remove right side */
        char *expected;
    };

    struct strcut_test_case_2 cases_2[] = {
        {"ALPHA.BETA", '.', 0, "BETA"},
        {"ALPHA.BETA", '.', 1, "ALPHA"},
        {"ALPHA.BETA", 'B', 0, "ETA"},
        {"ALPHA.BETA", 'B', 1, "ALPHA."},
        {"ALPHA.BETA", 'A', 0, "LPHA.BETA"},
        {"ALPHA.BETA", 'A', 1, ""},
        {"ALPHA.BETA.GAMMA", '.', 0, "BETA.GAMMA"},
        {"ALPHA.BETA.GAMMA", '.', 1, "ALPHA"},
    };

    printf("TEST 2: ");

    n = sizeof(cases_2) / sizeof(struct strcut_test_case_2);
    for (i = 0; i < n; ++i) {
        test_strcut_second(cases_2[i].in, cases_2[i].sep, cases_2[i].side, cases_2[i].expected);
    }
    printf("OK\n");
}

int main() {
    test_strcut();
    return 0;
}
