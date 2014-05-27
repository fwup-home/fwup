#ifndef CFGFILE_H
#define CFGFILE_H

#include <confuse.h>

int cfgfile_parse_file(const char *filename, cfg_t **cfg);
int cfgfile_parse_buffer(const char *buffer, cfg_t **cfg);
void cfgfile_free(cfg_t *cfg);

#endif // CFGFILE_H
