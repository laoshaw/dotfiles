#if 0
exec ${CC:-cc} -Weverything -Wno-vla $CFLAGS -o $(dirname $0)/xx $0
#endif

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static bool zero(const uint8_t *buf, size_t len) {
    for (size_t i = 0; i < len; ++i)
        if (buf[i]) return false;
    return true;
}

enum {
    FLAG_ASCII = 1,
    FLAG_OFFSET = 2,
    FLAG_SKIP = 4,
};

int main(int argc, char **argv) {
    size_t cols = 16;
    size_t group = 8;
    uint8_t flags = FLAG_ASCII | FLAG_OFFSET;
    char *path = NULL;

    while (getopt(argc, argv, "ac:fg:hk") > 0) {
        if (optopt == 'a')
            flags ^= FLAG_ASCII;
        else if (optopt == 'c')
            cols = (size_t) strtol(optarg, NULL, 10);
        else if (optopt == 'f')
            flags ^= FLAG_OFFSET;
        else if (optopt == 'g')
            group = (size_t) strtol(optarg, NULL, 10);
        else if (optopt == 'k')
            flags ^= FLAG_SKIP;
        else {
            printf("usage: xx [-afk] [-c N] [-g N] [FILE]\n");
            return (optopt == 'h') ? EXIT_SUCCESS : EXIT_FAILURE;
        }
    }
    if (!cols) return EXIT_FAILURE;
    if (argc > optind)
        path = argv[optind];

    FILE *file = path ? fopen(path, "r") : stdin;
    if (!file) {
        perror(path);
        return EXIT_FAILURE;
    }

    uint8_t buf[cols];
    size_t offset = 0, len = 0, i;
    for (;;) {
        offset += len;
        len = fread(buf, 1, sizeof(buf), file);
        if (!len) break;

        if ((flags & FLAG_SKIP) && len == sizeof(buf)) {
            static bool skip = false;
            if (zero(buf, len)) {
                if (!skip) printf("*\n");
                skip = true;
                continue;
            }
            skip = false;
        }

        if (flags & FLAG_OFFSET)
            printf("%08zx:  ", offset);

        for (i = 0; i < len; ++i) {
            if (group && i && !(i % group)) printf(" ");
            printf("%02x ", buf[i]);
        }

        if (flags & FLAG_ASCII) {
            for (i = len; i < cols; ++i)
                printf((group && !(i % group)) ? "    " : "   ");
            printf(" ");
            for (i = 0; i < len; ++i)
                printf("%c", isprint(buf[i]) ? buf[i] : '.');
        }

        printf("\n");
        if (len < sizeof(buf)) break;
    }

    if (ferror(file)) {
        perror(path);
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
