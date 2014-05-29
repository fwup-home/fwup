/*
 * Copyright 2014 LKC Technologies, Inc.
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

#ifndef UTIL_H
#define UTIL_H

void set_now_time();
void set_last_error(const char *msg);
const char *last_error();

#define NUM_ELEMENTS(X) (sizeof(X) / sizeof(X[0]))

#define ERR_RETURN(MSG) do { set_last_error(MSG); return -1; } while (0)

#endif // UTIL_H
