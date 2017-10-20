/*
 * Copyright 2016-2017 Frank Hunleth
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval_math.h"
#include "util.h"
#include <stdlib.h>
#include <inttypes.h>

#define EVAL_STACK_SIZE 16

enum associativity {
    ASSOC_LEFT,
    ASSOC_RIGHT
};

struct eval_state;
struct operator_token {
    int (*fun)(struct eval_state *);
    int prec;
};

struct eval_state {
    int (*parse)(struct eval_state*);
    const char *str;

    int64_t args[EVAL_STACK_SIZE];
    int64_t *next_arg;

    struct operator_token operator_stack[EVAL_STACK_SIZE];
    struct operator_token *next_op;
};

#define LEFT_PAREN ((int (*)(struct eval_state*))NULL)

static int arg_count(struct eval_state *s)
{
    return s->next_arg - s->args;
}

static int op_count(struct eval_state *s)
{
    return s->next_op - s->operator_stack;
}

#define require_args(s, n) if (arg_count(s) < n) return -1;

static int do_add(struct eval_state *s)
{
    require_args(s, 2);
    s->next_arg[-2] += s->next_arg[-1];
    s->next_arg--;
    return 0;
}

static int do_subtract(struct eval_state *s)
{
    require_args(s, 2);
    s->next_arg[-2] -= s->next_arg[-1];
    s->next_arg--;
    return 0;
}

static int do_multiply(struct eval_state *s)
{
    require_args(s, 2);
    s->next_arg[-2] *= s->next_arg[-1];
    s->next_arg--;
    return 0;
}

static int do_divide(struct eval_state *s)
{
    require_args(s, 2);

    if (s->next_arg[-1] == 0)
        ERR_RETURN("div by 0");

    s->next_arg[-2] /= s->next_arg[-1];
    s->next_arg--;
    return 0;
}

static int do_unary_minus(struct eval_state *s)
{
    require_args(s, 1);
    s->next_arg[-1] = -s->next_arg[-1];
    return 0;
}

static int do_power(struct eval_state *s)
{
    require_args(s, 2);

    int64_t base = s->next_arg[-2];
    int64_t exponent = s->next_arg[-1];

    if (exponent < 0 || exponent > 63)
        ERR_RETURN("exponents out of range");

    int64_t result = 1;
    while (exponent > 0) {
        result *= base;
        exponent--;
    }

    s->next_arg[-2] = result;
    s->next_arg--;
    return 0;
}

static int pop_operator(struct eval_state *s)
{
    if (op_count(s) <= 0)
        return -1;

    struct operator_token *op = s->next_op - 1;

    if (op->fun == LEFT_PAREN)
        ERR_RETURN("extra left paren");

    int rc = op->fun(s);
    s->next_op--;
    return rc;
}

static int handle_right_paren(struct eval_state *s)
{
    for (;;) {
        if (op_count(s) == 0)
            ERR_RETURN("extra right paren");

        struct operator_token *op = s->next_op - 1;
        if (op->fun == LEFT_PAREN) {
            s->next_op--;
            return 0;
        }

        if (pop_operator(s) < 0)
            return -1;
    }
}

static int handle_left_paren(struct eval_state *s)
{
    if (op_count(s) >= EVAL_STACK_SIZE)
        ERR_RETURN("too many ops");

    s->next_op->fun = LEFT_PAREN;
    s->next_op->prec = 1;
    s->next_op++;
    return 0;
}

static int push_operator(struct eval_state *s,
                         int (*fun)(struct eval_state*),
                         enum associativity assoc,
                         int precedence)
{
    while (s->next_op != s->operator_stack) {
        struct operator_token *op = s->next_op - 1;

        // Check for a higher precedence operator on the stack
        if ((assoc == ASSOC_LEFT && precedence > op->prec) ||
                (assoc == ASSOC_RIGHT && precedence >= op->prec))
            break;

        // If not, it's time to pop the operator off
        if (pop_operator(s) < 0)
            return -1;
    }

    s->next_op->fun = fun;
    s->next_op->prec = precedence;
    s->next_op++;
    return 0;
}

static int handle_suffix(struct eval_state *s,
                         int64_t multiplier)
{
    // Suffixes are unary and have highest precedence,
    // so don't bother pushing them on the operator stack.
    require_args(s, 1);

    s->next_arg[-1] *= multiplier;
    return 0;
}

static int parse_operator(struct eval_state *s);
static int parse_number(struct eval_state *s);

static int parse_whitespace_before_number(struct eval_state *s)
{
    int rc;
    switch (*s->str) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        s->str++;
        rc = 0;
        break;

    case '(':
        rc = handle_left_paren(s);
        s->str++;
        break;

    case ')':
        rc = handle_right_paren(s);
        s->str++;
        break;

    case '-':
        rc = push_operator(s, do_unary_minus, ASSOC_RIGHT, 4);
        s->str++;
        break;

    default:
        s->parse = parse_number;
        rc = parse_number(s);
        break;
    }

    return rc;
}

static int parse_number(struct eval_state *s)
{
    char *endptr;
    int64_t result = strtoll(s->str, &endptr, 0);
    if (endptr == s->str)
        ERR_RETURN("parse error");

    if (arg_count(s) >= EVAL_STACK_SIZE)
        ERR_RETURN("argument overflow");

    *s->next_arg = result;
    s->next_arg++;
    s->parse = parse_operator;
    s->str = endptr;
    return 0;
}

static int parse_operator(struct eval_state *s)
{
    int rc = 0;
    switch (*s->str) {
    case ' ':
    case '\t':
    case '\r':
    case '\n':
        break;

    case '+':
        rc = push_operator(s, do_add, ASSOC_LEFT, 2);
        s->parse = parse_whitespace_before_number;
        break;
    case '-':
        rc = push_operator(s, do_subtract, ASSOC_LEFT, 2);
        s->parse = parse_whitespace_before_number;
        break;
    case '*':
        rc = push_operator(s, do_multiply, ASSOC_LEFT, 3);
        s->parse = parse_whitespace_before_number;
        break;
    case '/':
        rc = push_operator(s, do_divide, ASSOC_LEFT, 3);
        s->parse = parse_whitespace_before_number;
        break;
    case '^':
        rc = push_operator(s, do_power, ASSOC_RIGHT, 4);
        s->parse = parse_whitespace_before_number;
        break;
    case '(':
        rc = handle_left_paren(s);
        break;
    case ')':
        rc = handle_right_paren(s);
        break;

        // N and BYTES may be followed by the following multiplicative suffixes: c =1, w =2, b =512, kB =1000, K =1024,  MB
        //      =1000*1000, M =1024*1024, xM =M GB =1000*1000*1000, G =1024*1024*1024, and so on for T, P, E, Z, Y.
    case 'c':
        // This may seem silly, but it at least checks that there's
        // something on the stack.
        rc = handle_suffix(s, 1);
        break;
    case 'w':
        rc = handle_suffix(s, 2);
        break;
    case 'b':
        rc = handle_suffix(s, 512);
        break;
    case 'k':
        if (s->str[1] == 'B') {
            rc = handle_suffix(s, 1000);
            s->str++;
        } else {
            rc = -1;
        }
        break;
    case 'K':
        rc = handle_suffix(s, 1024);
        break;
    case 'M':
        if (s->str[1] == 'B') {
            rc = handle_suffix(s, 1000 * 1000);
            s->str++;
        } else {
            rc = handle_suffix(s, 1024 * 1024);
        }
        break;
    case 'G':
        if (s->str[1] == 'B') {
            rc = handle_suffix(s, 1000 * 1000 * 1000);
            s->str++;
        } else {
            rc = handle_suffix(s, 1024 * 1024 * 1024);
        }
        break;

    default:
        ERR_RETURN("invalid character");
    }
    s->str++;
    return rc;
}

/**
 * Evaluate the expression in the specified string and return the result.
 *
 * Expressions are evaluated using integer math. The following operations
 * are supported: +, -, *, /, and ^ (power). Additionally, numbers may be
 * suffixed with dd(1) suffixes to specify multipliers like K (*1024) or
 * kB (*1000), etc.
 *
 * @param str the input string
 * @param result the result
 * @return -1 if there's a parse error, 0 on success
 */
int eval_math(const char *str, int64_t *result)
{
    struct eval_state state;
    state.next_arg = state.args;
    state.next_op = state.operator_stack;
    state.str = str;
    state.parse = parse_whitespace_before_number;

    // Parse the string
    while (*state.str) {
        if (state.parse(&state) < 0)
            return -1;
    }

    // Handle the remaining stack
    while (state.next_op != state.operator_stack) {
        if (pop_operator(&state) < 0)
            return -1;
    }

    if (arg_count(&state) != 1)
        ERR_RETURN("eval error");

    *result = state.args[0];
    return 0;
}

/**
 * Evaluate the specified string and store the result as a string.
 * This is a helper function for eval_math().
 *
 * @param str the input string
 * @param result_str a place to store the result
 * @param result_str_len the length of the result_str
 * @return -1 if there's a parse error, 0 on success
 */
int eval_math_str(const char *str, char *result_str, int result_str_len)
{
    int64_t result;
    if (eval_math(str, &result) < 0)
        return -1;

    if (snprintf(result_str, result_str_len, "%" PRId64, result) >= result_str_len)
        return -1;

    return 0;
}
