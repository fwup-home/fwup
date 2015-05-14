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
#include <sodium.h>

#include "mmc.h"
#include "util.h"
#include "fwup_apply.h"
#include "fwup_create.h"
#include "fwup_list.h"
#include "fwup_metadata.h"
#include "fwup_genkeys.h"
#include "fwup_sign.h"
#include "fwup_verify.h"
#include "config.h"

// Global options
bool fwup_verbose = false;
static bool numeric_progress = false;
static bool quiet = false;

static void print_version()
{
    fprintf(stderr, "%s version %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

static void print_usage(const char *argv0)
{
    print_version();
    fprintf(stderr, "\n");
    fprintf(stderr, "Usage: %s [options]\n", argv0);
    fprintf(stderr, "  -a   Apply the firmware update\n");
    fprintf(stderr, "  -c   Create the firmware update\n");
    fprintf(stderr, "  -d <Device file for the memory card>\n");
    fprintf(stderr, "  -f <fwupdate.conf> Specify the firmware update configuration file\n");
    fprintf(stderr, "  -g Generate firmware signing keys (fwup-key.pub and fwup-key.priv)\n");
    fprintf(stderr, "  -i <input.fw> Specify the input firmware update file (Use - for stdin)\n");
    fprintf(stderr, "  -l   List the available tasks in a firmware update\n");
    fprintf(stderr, "  -m   Print metadata in the firmware update\n");
    fprintf(stderr, "  -n   Report numeric progress\n");
    fprintf(stderr, "  -o <output.fw> Specify the output file when creating an update (Use - for stdout)\n");
    fprintf(stderr, "  -q   Quiet\n");
    fprintf(stderr, "  -s <keyfile> A private key file for signing firmware updates\n");
    fprintf(stderr, "  -S Sign an existing firmware file (specify -i and -o)\n");
    fprintf(stderr, "  -t <task> Task to apply within the firmware update\n");
    fprintf(stderr, "  -v   Verbose\n");
    fprintf(stderr, "  -V Verify an existing firmware file (specify -i)\n");
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
    fprintf(stderr, "\n");
    fprintf(stderr, "Generate a public/private key pair and sign a firmware archive:\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "  $ %s -g\n", argv0);
    fprintf(stderr, "  (Store fwup-key.priv is a safe place. Store fwup-key.pub on the target)\n");
    fprintf(stderr, "  $ %s -S -s fwup-key.priv -i myfirmware.fw -o signedfirmware.fw\n", argv0);
}

#define CMD_NONE    0
#define CMD_APPLY   1
#define CMD_CREATE  2
#define CMD_LIST    3
#define CMD_METADATA 4
#define CMD_GENERATE_KEYS 5
#define CMD_SIGN    6
#define CMD_VERIFY  7

int main(int argc, char **argv)
{
    const char *configfile = "fwupdate.conf";
    int command = CMD_NONE;

    const char *mmc_device = NULL;
    const char *input_firmware = NULL;
    const char *output_firmware = NULL;
    const char *task = NULL;
    bool accept_found_device = false;
    unsigned char *signing_key = NULL;
    unsigned char *public_key = NULL;

    if (argc == 1) {
        print_usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    static struct option long_options[] = {
        {"apply",   no_argument,    0, 'a'},
        {"create",  no_argument,    0, 'c'},
        {"gen-keys", no_argument,   0, 'g'},
        {"list",    no_argument,    0, 'l'},
        {"metadata", no_argument,   0, 'm'},
        {"sign", no_argument,       0, 'S'},
        {"verify", no_argument,     0, 'V'},
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "acd:f:gi:lmno:p:qSs:t:Vvyz", long_options, NULL)) != -1) {
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
        case 'g':
            command = CMD_GENERATE_KEYS;
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
        case 'p':
        {
            FILE *fp = fopen(optarg, "rb");
            public_key = (unsigned char *) malloc(crypto_sign_PUBLICKEYBYTES);
            if (!fp || fread(public_key, 1, crypto_sign_PUBLICKEYBYTES, fp) != crypto_sign_PUBLICKEYBYTES)
                err(EXIT_FAILURE, "Error reading public key from file '%s'", optarg);
            fclose(fp);
            break;
        }
        case 'n':
            numeric_progress = true;
            break;
        case 'q':
            quiet = true;
            break;
        case 'S':
            command = CMD_SIGN;
            break;
        case 's':
        {
            FILE *fp = fopen(optarg, "rb");
            signing_key = (unsigned char *) malloc(crypto_sign_SECRETKEYBYTES);
            if (!fp || fread(signing_key, 1, crypto_sign_SECRETKEYBYTES, fp) != crypto_sign_SECRETKEYBYTES)
                err(EXIT_FAILURE, "Error reading signing key from file '%s'", optarg);
            fclose(fp);
            break;
        }
        case 't':
            task = optarg;
            break;
        case 'v':
            fwup_verbose = true;
            break;
        case 'V':
            command = CMD_VERIFY;
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
        errx(EXIT_FAILURE, "specify one of -a, -c, -l, -m, -S, -V, or -z");
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

        // unmount everything using the device to avoid corrupting partitions
        mmc_umount_all(mmc_device);
        if (fwup_apply(input_firmware,
                       task,
                       mmc_device,
                       quiet ? FWUP_APPLY_NO_PROGRESS : numeric_progress ? FWUP_APPLY_NUMERIC_PROGRESS : FWUP_APPLY_NORMAL_PROGRESS,
                       public_key) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_CREATE:
        if (fwup_create(configfile, output_firmware, signing_key) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_LIST:
        if (fwup_list(input_firmware, public_key) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_METADATA:
        if (fwup_metadata(input_firmware, public_key) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_GENERATE_KEYS:
        if (fwup_genkeys() < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_SIGN:
        if (fwup_sign(input_firmware, output_firmware, signing_key) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;

    case CMD_VERIFY:
        if (fwup_verify(input_firmware, public_key) < 0)
            errx(EXIT_FAILURE, "%s", last_error());

        break;
    }

    return 0;
}

