#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef _WIN32
#include <fcntl.h> // O_BINARY

// Assume that all windows platforms are little endian
#define TO_BIGENDIAN32(X) _byteswap_ulong(X)
#define FROM_BIGENDIAN32(X) _byteswap_ulong(X)
#else
// Other platforms have htons and ntohs without pulling in another library
#include <arpa/inet.h>
#define TO_BIGENDIAN32(X) htonl(X)
#define FROM_BIGENDIAN32(X) ntohl(X)
#endif

static bool verbose = false;

static void usage()
{
    printf("Usage: framing-helper [OPTION]...\n");
    printf("\n");
    printf("Options:\n");
    printf("  -d  Verify and remove framing\n");
    printf("  -e  Add framing\n");
    printf("  -n <value>  Max frame size when adding framing (default 4096)\n");
    printf("  -v  Verbose\n");
}

static void remove_framing()
{
    static const size_t buffer_size = 4096;
    char *buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "malloc failed.\n");
        exit(EXIT_FAILURE);
    }

    size_t current_frame_remaining = 0;
    for (;;) {
        size_t amount_to_read = current_frame_remaining;
        if (amount_to_read == 0)
            amount_to_read = sizeof(uint32_t);
        else if (amount_to_read > buffer_size)
            amount_to_read = buffer_size;

        size_t amount_read = fread(buffer, 1, amount_to_read, stdin);

        // Got a real EOF and not in the middle of a frame.
        if (current_frame_remaining == 0 && amount_read == 0)
            break;

        // Check that the amount read matches the expected.
        if (amount_read != amount_to_read) {
            fprintf(stderr, "Expected to read %u bytes, but only got %u bytes.\n",
                 (unsigned int) amount_to_read, (unsigned int) amount_read);
            exit(EXIT_FAILURE);
        }

        if (current_frame_remaining == 0) {
            // Reading frame header
            current_frame_remaining = FROM_BIGENDIAN32(*((uint32_t*) buffer));

            if (verbose)
                fprintf(stderr, "Going to read %u byte frame\n", (unsigned int) current_frame_remaining);

            // Check for EOF marker
            if (current_frame_remaining == 0)
                break;
        } else {
            fwrite(buffer, 1, amount_read, stdout);
            current_frame_remaining -= amount_read;
        }
    }

    free(buffer);
}

static void add_framing(size_t frame_size)
{
    char *buffer = malloc(frame_size);
    if (!buffer) {
        fprintf(stderr, "malloc failed.\n");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        size_t amount_read = fread(buffer, 1, frame_size, stdin);
        if (amount_read == 0)
            break;

        if (verbose)
            fprintf(stderr, "Writing %u byte frame\n", (unsigned int) amount_read);

        uint32_t be_len = TO_BIGENDIAN32(amount_read);
        fwrite(&be_len, sizeof(be_len), 1, stdout);
        fwrite(buffer, 1, amount_read, stdout);
        if (amount_read != frame_size)
            break;
    };

    if (verbose)
        fprintf(stderr, "Writing EOF frame\n");

    // Write an EOF frame (0 length)
    uint32_t zeros = 0;
    fwrite(&zeros, sizeof(zeros), 1, stdout);

    free(buffer);
}

int main(int argc, char *argv[])
{
    enum {
        ADD_FRAMING,
        REMOVE_FRAMING
    } command = ADD_FRAMING;
    size_t frame_size = 4096;

#ifdef _WIN32
    setmode(STDIN_FILENO, O_BINARY);
    setmode(STDOUT_FILENO, O_BINARY);
#endif

    int opt;
    while ((opt = getopt(argc, argv, "den:v")) != -1) {
        switch (opt) {
        case 'd': // Remove framing
            command = REMOVE_FRAMING;
            break;

        case 'e': // Add framing
            command = ADD_FRAMING;
            break;

        case 'n': // Frame size
            frame_size = strtoul(optarg, NULL, 0);
            break;

        case 'v': // Verbose
            verbose = true;
            break;

        default:
            usage();
            exit(EXIT_FAILURE);
        }
    }

    switch (command) {
    case REMOVE_FRAMING:
        remove_framing();
        break;

    case ADD_FRAMING:
        add_framing(frame_size);
        break;
    }
    exit(EXIT_SUCCESS);
}
