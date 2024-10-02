// This file is dual-licensed.  Choose whichever licence you want from
// the two licences listed below.
//
// The first licence is a regular 2-clause BSD licence.  The second licence
// is the CC-0 from Creative Commons. It is intended to release Monocypher
// to the public domain.  The BSD licence serves as a fallback option.
//
// SPDX-License-Identifier: BSD-2-Clause OR CC0-1.0
//
// ------------------------------------------------------------------------
//
// Copyright (c) 2017-2019, Loup Vaillant
// All rights reserved.
//
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ------------------------------------------------------------------------
//
// Written in 2017-2019 by Loup Vaillant
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related neighboring rights to this software to the public domain
// worldwide.  This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software.  If not, see
// <https://creativecommons.org/publicdomain/zero/1.0/>

#ifndef UTILS_H
#define UTILS_H

#include <inttypes.h>
#include <stddef.h>

typedef int8_t   i8;
typedef uint8_t  u8;
typedef uint32_t u32;
typedef int32_t  i32;
typedef int64_t  i64;
typedef uint64_t u64;

#define FOR(i, start, end) for (size_t i = (start); i < (end); i++)
#define SODIUM_INIT                                     \
    do {                                                \
        if (sodium_init() == -1) {                      \
            printf("libsodium init failed.  Abort.\n"); \
            return 1;                                   \
        }                                               \
    } while (0)
#define RANDOM_INPUT(name, size) u8 name[size]; p_random(name, size)

extern u64 random_state; // state of the RNG

typedef struct {
    u8     *buf;
    size_t  size;
} vector;

typedef struct {
    const char **next;
    size_t       size;
    vector       inputs[10];
    size_t       nb_inputs;
    vector       expected;
    vector       out;
} vector_reader;

void store64_le(u8 out[8], u64 in);
u64  load64_le(const u8 s[8]);
u32  load32_le(const u8 s[4]);
u64  rand64(void); // Pseudo-random 64 bit number, based on xorshift*
void p_random(u8 *stream, size_t size);
void print_vector(const u8 *buf, size_t size);
void print_number(u64 n);
void* alloc(size_t size);

vector next_input (vector_reader *vectors);
vector next_output(vector_reader *vectors);
int vector_test(void (*f)(vector_reader*),
                const char *name, size_t nb_vectors, const char *vectors[]);

#endif // UTILS_H
