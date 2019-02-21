#include "filter_cfg.h"
#include "resources.h"
#include "util.h"

int filter_cfg(cfg_t *cfg, const char **include_tasks)
{
    struct resource_list *all_resources = NULL;
    struct resource_list *used_resources = NULL;
    int rc = 0;
    unsigned num_tasks = cfg_size(cfg, "task");
    const char *unused_tasks[num_tasks];
    unsigned num_unused_tasks = 0;

    if (*include_tasks == NULL)
        return 0;

    // Find unused tasks
    for (unsigned ix = 0; ix < num_tasks; ix++) {
        cfg_t *task = cfg_getnsec(cfg, "task", ix);

        const char *task_name = cfg_title(task);
        if (strncmp(task_name, *include_tasks, 7) != 0) {
            unused_tasks[num_unused_tasks] = task_name;
            num_unused_tasks++;
        }
    }

    // Remove unused tasks
    for (unsigned ix = 0; ix < num_unused_tasks; ix++)
        cfg_rmtsec(cfg, "task", unused_tasks[ix]);

    // Find unused resources
    OK_OR_CLEANUP(rlist_get_all(cfg, &all_resources));
    OK_OR_CLEANUP(rlist_get_used(cfg, &used_resources));
    rlist_subtract(&all_resources, used_resources);

    // Remove the unused resources
    for (struct resource_list *list = all_resources; list; list = list->next)
        cfg_rmtsec(cfg, "file-resource", cfg_title(list->resource));

cleanup:
    rlist_free(all_resources);
    rlist_free(used_resources);
    return rc;
}
