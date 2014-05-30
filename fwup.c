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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <err.h>
#include <getopt.h>

#include "mmc.h"
#include "util.h"
#include "fwup_apply.h"
#include "fwup_create.h"
#include "fwup_list.h"
#include "fwup_metadata.h"

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

    // Normalize the firmware filenames in the case that the user wants
    // to use stdin/stdout
    if (input_firmware && strcmp(input_firmware, "-") == 0)
        input_firmware = 0;
    if (output_firmware && strcmp(output_firmware, "-") == 0)
        output_firmware = 0;

    switch (command) {
    case CMD_NONE:
        errx(EXIT_FAILURE, "specify one of -a, -c, -l, -m, or -z");
        break;

    case CMD_APPLY:
        if (!task)
            errx(EXIT_FAILURE, "specify a task (-t)");

        if (!mmc_device) {
            mmc_device = mmc_find_device();
            if (!accept_found_device) {
                if (strcmp(input_firmware, "-") == 0)
                    errx(EXIT_FAILURE, "Cannot confirm use of %s when using stdin.\nRerun with -y if location is correct.", mmc_device);

                char sizestr[16];
                mmc_pretty_size(mmc_device_size(mmc_device), sizestr);
                fprintf(stderr, "Use %s memory card found at %s? [y/N] ", sizestr, mmc_device);
                int response = fgetc(stdin);
                if (response != 'y' && response != 'Y')
                    errx(EXIT_FAILURE, "aborted");
            }
        }
        if (fwup_apply(input_firmware, task, mmc_device) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_CREATE:
        if (fwup_create(configfile, output_firmware) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_LIST:
        if (fwup_list(input_firmware) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_METADATA:
        if (fwup_metadata(input_firmware) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;
    }

    return 0;
}

