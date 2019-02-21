/*
 * Copyright 2017 Frank Hunleth
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

#ifndef RESOURCES_H
#define RESOURCES_H

#include <stdbool.h>
#include <confuse.h>

struct resource_list {
    struct resource_list *next;
    cfg_t *resource;
    bool processed;
};

int rlist_get_all(cfg_t *cfg, struct resource_list **resources);
int rlist_get_from_task(cfg_t *cfg, cfg_t *task, struct resource_list **resources);
int rlist_get_used(cfg_t *cfg, struct resource_list **resources);
void rlist_free(struct resource_list *list);
void rlist_subtract(struct resource_list **list, struct resource_list *what);
struct resource_list *rlist_find_by_name(struct resource_list *list, const char *name);

#endif // RESOURCES_H
