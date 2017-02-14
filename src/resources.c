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

#include "resources.h"
#include "util.h"

#include <stdlib.h>
#include <string.h>

/**
 * @brief Create a list of all resources in an archive
 *
 * @param cfg the meta.conf configuration
 * @param resources the list of resources (could be NULL if none referenced in task)
 * @return 0 on success
 */
int rlist_get_all(cfg_t *cfg, struct resource_list **resources)
{
    struct resource_list *list = NULL;
    for (unsigned ix = 0;; ix++) {
        cfg_t *resource = cfg_getnsec(cfg, "file-resource", ix);
        if (!resource)
            break;

        struct resource_list *new_node = (struct resource_list *) malloc(sizeof(struct resource_list));
        new_node->next = list;
        new_node->resource = resource;
        new_node->processed = false;
        list = new_node;
    }
    *resources = list;
    return 0;
}

/**
 * @brief Create a list of all resources referenced by a task
 * @param cfg the meta.conf configuration
 * @param task the desired task
 * @param resources the list of resources (could be NULL if none referenced in task)
 * @return 0 on success
 */
int rlist_get_from_task(cfg_t *cfg, cfg_t *task, struct resource_list **resources)
{
    struct resource_list *list = NULL;
    for (unsigned ix = 0;; ix++) {
        cfg_t *onresource = cfg_getnsec(task, "on-resource", ix);
        if (!onresource)
            break;

        struct resource_list *new_node = (struct resource_list *) malloc(sizeof(struct resource_list));
        new_node->next = list;

        const char *resource_name = cfg_title(onresource);
        cfg_t *resource = cfg_gettsec(cfg, "file-resource", resource_name);
        if (resource == NULL) {
            rlist_free(list);
            ERR_RETURN("Resource '%s' used, but metadata is missing. Archive is corrupt.", resource_name);
        }

        new_node->resource = resource;
        new_node->processed = false;
        list = new_node;
    }
    *resources = list;
    return 0;
}

/**
 * @brief Free the specified resource list
 *
 * @param list a resource list
 */
void rlist_free(struct resource_list *list)
{
    while (list) {
        struct resource_list *next = list->next;
        free(list);
        list = next;
    }
}

/**
 * @brief Find a resource by name in the list
 *
 * @param list a resource list
 * @param name the name of the resource
 *
 * @return the resource information or NULL if not found
 */
struct resource_list *rlist_find_by_name(struct resource_list *list, const char *name)
{
    while (list) {
        if (strcmp(name, cfg_title(list->resource)) == 0)
            return list;
        list = list->next;
    }
    return NULL;
}
