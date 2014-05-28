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

#include <confuse.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>

#include "cfgfile.h"
#include "util.h"
#include "mmc.h"
#include "fwup_create.h"

#include <archive.h>
#include <archive_entry.h>
#include "sha2.h"

// Global options
static bool numeric_progress = false;
static bool quiet = false;

//FIXME!!
#define PACKAGE_NAME "fwup"
#define PACKAGE_VERSION "0.1"

static void print_version()
{
    fprintf(stderr, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

static void print_usage(const char *argv0)
{
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "  -a   Apply the firmware update\n");
    fprintf(stderr, "  -c   Create the firmware update\n");
    fprintf(stderr, "  -d <Device file for the memory card>\n");
    fprintf(stderr, "  -f <fwupdate.conf> Specify the firmware update configuration file\n");
    fprintf(stderr, "  -i <input.fw> Specify the input firmware update file (Use - for stdin)\n");
    fprintf(stderr, "  -l   List the available tasks in a firmware update\n");
    fprintf(stderr, "  -m   Print metadata in the firmware update\n");
    fprintf(stderr, "  -n   Report numeric progress\n");
    fprintf(stderr, "  -o <output.fw> Specify the output file when creating an update (Use - for stdout)\n");
    fprintf(stderr, "  -q   Quiet\n");
    fprintf(stderr, "  -t <task> Task to apply within the firmware update\n");
    fprintf(stderr, "  -v   Print version and exit\n");
    fprintf(stderr, "  -y   Accept automatically found memory card when applying a firmware update\n");
    fprintf(stderr, "  -z   Print the memory card that would be automatically detected and exit\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Examples:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Create a firmware update archive:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ %s -c -f fwupdate.conf -o myfirmware.fw\n", argv0);
    fprintf(stderr, "\n");
    fprintf(stderr, "Apply the firmware update to /dev/sdc and specify the 'upgrade' task:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ %s -a -d /dev/sdc -i myfirmware.fw -t upgrade\n", argv0);
}

#define CMD_NONE    0
#define CMD_APPLY   1
#define CMD_CREATE  2
#define CMD_LIST    3
#define CMD_METADATA 4

int main(int argc, char **argv)
{
    const char *configfile = "fwupdate.conf";
    int command = CMD_NONE;

    const char *mmc_device = NULL;
    const char *input_firmware = NULL;
    const char *output_firmware = NULL;
    const char *task = NULL;
    bool accept_found_device = false;

    if (argc == 1) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    int opt;
    while ((opt = getopt(argc, argv, "acd:f:i:lmno:pqt:yz")) != -1) {
        switch (opt) {
        case 'a':
            command = CMD_APPLY;
            break;
        case 'c':
            command = CMD_CREATE;
            break;
        case 'd':
            mmc_device = optarg;
            break;
        case 'f':
            configfile = optarg;
            break;
        case 'i':
            input_firmware = optarg;
            break;
        case 'l':
            command = CMD_LIST;
            break;
        case 'm':
            command = CMD_METADATA;
            break;
        case 'o':
            output_firmware = optarg;
            break;
        case 'n':
            numeric_progress = true;
            break;
        case 'q':
            quiet = true;
            break;
        case 't':
            task = optarg;
            break;
        case 'v':
            print_version();
            exit(EXIT_SUCCESS);
            break;
        case 'y':
            accept_found_device = true;
            break;
        case 'z':
            mmc_device = mmc_find_device();
            printf("%s", mmc_device);
            exit(EXIT_SUCCESS);
            break;
        default: /* '?' */
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (quiet && numeric_progress)
        errx(EXIT_FAILURE, "pick either -n or -q, but not both");

    if (optind < argc)
        errx(EXIT_FAILURE, "unexpected parameter: %s", argv[optind]);

    switch (command) {
    case CMD_NONE:
        errx(EXIT_FAILURE, "specify one of -a, -c, -l, -m, or -z");
        break;

    case CMD_APPLY:
        errx(EXIT_FAILURE, "not implemented");
        break;

    case CMD_CREATE:
        if (!output_firmware)
            errx(EXIT_FAILURE, "specify the output firmware file name");

        fwup_create(configfile, output_firmware);
        break;

    case CMD_LIST:
        errx(EXIT_FAILURE, "not implemented");
        break;

    case CMD_METADATA:
        errx(EXIT_FAILURE, "not implemented");
        break;
    }

#if 0
    /* print the parsed values to another file */
    {        
        struct archive *a = archive_write_new();
        archive_write_set_format_zip(a);
        if (archive_write_open_filename(a, "test.zip") != ARCHIVE_OK) {
            fprintf(stderr, "Error writing to .zip file");
            return 3;
        }
        struct archive_entry *entry = archive_entry_new();
        archive_entry_set_pathname(entry, "meta.conf");
        archive_entry_set_size(entry, configtxt_len);
        archive_entry_set_filetype(entry, AE_IFREG);
        archive_entry_set_perm(entry, 0644);
        archive_write_header(a, entry);
        archive_write_data(a, configtxt, configtxt_len);
        archive_entry_free(entry);

        archive_write_close(a);
        archive_write_free(a);
        free(configtxt);
    }
#endif

    return 0;
}

