/* Neutral public-API fixtures: no external assets or window needed. */
#include "Genesis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int write_member(geVFile *archive, const char *name, const void *data, int size)
{
    geVFile *file = geVFile_Open(archive, name, GE_VFILE_OPEN_CREATE);
    int ok;
    if (!file) return 0;
    ok = geVFile_Write(file, data, size);
    return geVFile_Close(file) && ok;
}

static int check_lines(geVFile *file, const char *expected, int capacity, long size)
{
    char line[256], joined[1024] = {0};
    size_t used = 0;
    long before = 0, after = 0;
    int iteration, invalid, ok = 0;
    if (!file) return 0;
    /* Invalid capacities neither read nor disturb the caller's buffer. */
    for (invalid = -1; invalid <= 1; ++invalid) {
        line[0] = 'Q';
        if (geVFile_GetS(file, line, invalid) || !geVFile_Tell(file, &after) || after != 0 || line[0] != 'Q') goto done;
    }
    for (iteration = 0; iteration < 512; ++iteration) {
        int got;
        size_t count;
        if (!geVFile_Tell(file, &before)) goto done;
        memset(line, 0xa5, sizeof(line));
        got = geVFile_GetS(file, line, capacity);
        if (!geVFile_Tell(file, &after)) goto done;
        if (!got) { ok = after == size && !strcmp(joined, expected); break; }
        if (after <= before || after > size || !memchr(line, 0, (size_t)capacity)) goto done;
        count = strlen(line);
        if (used + count >= sizeof(joined) || strchr(line, '\r')) goto done;
        memcpy(joined + used, line, count + 1);
        used += count;
    }
done:
    if (!ok) fprintf(stderr, "line contract failed: capacity=%d position=%ld size=%ld\n", capacity, after, size);
    return geVFile_Close(file) && ok;
}

static int line_cases(void)
{
    static const struct { const char *input, *output; int capacity; } cases[] = {
        {"a\r\nb\r\nlast", "a\nb\nlast", 132},
        {"a\nb\nlast", "a\nb\nlast", 132},
        {"a\rb\rlast", "a\nb\nlast", 132},
        {"final", "final", 132}, {"abcde", "abcde", 2},
        {"x", "x", 2}, {"x", "x", 132},
        {"abcdefghijk\nlast", "abcdefghijk\nlast", 4},
        {"", "", 132}, {"a\r\n", "a\n", 132},
        {"a\r", "a\n", 132}, {"\n", "\n", 2}
    };
    size_t i;
    int ok = 1;
    for (i=0; i<sizeof(cases)/sizeof(cases[0]); ++i) {
        geVFile *archive, *file;
        FILE *raw = fopen("loose.txt", "wb");
        int size = (int)strlen(cases[i].input);
        if (!raw) return 0;
        ok = fwrite(cases[i].input, 1, (size_t)size, raw) == (size_t)size && ok;
        ok = fclose(raw) == 0 && ok;
        archive = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_VIRTUAL, "lines.vfs", NULL,
            GE_VFILE_OPEN_CREATE | GE_VFILE_OPEN_DIRECTORY);
        if (!archive) return 0;
        file = geVFile_Open(archive, "member", GE_VFILE_OPEN_CREATE);
        if (!file) { geVFile_Close(archive); return 0; }
        if (size) ok = geVFile_Write(file, cases[i].input, size) && ok;
        ok = geVFile_Close(file) && ok;
        /* A following member detects reads escaping the current member. */
        ok = write_member(archive, "sentinel", "DO-NOT-READ", 11) && ok;
        ok = geVFile_Close(archive) && ok;
        file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_DOS, "loose.txt", NULL, GE_VFILE_OPEN_READONLY);
        ok = check_lines(file, cases[i].output, cases[i].capacity, size) && ok;
        archive = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_VIRTUAL, "lines.vfs", NULL,
            GE_VFILE_OPEN_READONLY | GE_VFILE_OPEN_DIRECTORY);
        if (!archive) return 0;
        file = geVFile_Open(archive, "member", GE_VFILE_OPEN_READONLY);
        ok = check_lines(file, cases[i].output, cases[i].capacity, size) && ok;
        ok = geVFile_Close(archive) && ok;
    }
    fprintf(stderr, "VFile lines: %u cases through each backend\n", (unsigned)(sizeof(cases)/sizeof(cases[0])));
    return ok;
}

static int check_reads(geVFile *file, int strict, const unsigned char *expected)
{
    unsigned char buffer[100];
    long position = -1;
    int ok = 1;
    if (!file) return 0;
    ok = geVFile_Read(file, buffer, 13) && !memcmp(buffer, expected, 13) && ok;
    memset(buffer, 0xa5, sizeof(buffer));
    ok = (geVFile_Read(file, buffer, 88) == (strict ? GE_FALSE : GE_TRUE)) && ok;
    ok = geVFile_Tell(file, &position) && position == (strict ? 13 : 100) && ok;
    if (strict) {
        int i;
        for (i=0; i<100; ++i) ok = buffer[i] == 0xa5 && ok;
    } else {
        ok = !memcmp(buffer, expected+13, 87) && buffer[87] == 0xa5 && ok;
        ok = !geVFile_Read(file, buffer, 1) && ok;
        ok = geVFile_Tell(file, &position) && position == 100 && ok;
    }
    ok = geVFile_Seek(file, 13, GE_VFILE_SEEKSET) && ok;
    ok = geVFile_Read(file, buffer, 87) && !memcmp(buffer, expected+13, 87) && ok;
    ok = geVFile_Tell(file, &position) && position == 100 && ok;
    ok = !geVFile_Read(file, buffer, 1) && ok;
    if (!ok) fprintf(stderr, "VFile read contract failed: strict=%d position=%ld\n", strict, position);
    return geVFile_Close(file) && ok;
}

static int read_cases(void)
{
    unsigned char data[100];
    geVFile *archive, *file;
    FILE *raw;
    int i, ok;
    for (i=0; i<100; ++i) data[i] = (unsigned char)i;
    raw = fopen("loose.bin", "wb");
    if (!raw) return 0;
    ok = fwrite(data, 1, sizeof(data), raw) == sizeof(data);
    ok = fclose(raw) == 0 && ok;
    file = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_DOS, "loose.bin", NULL, GE_VFILE_OPEN_READONLY);
    ok = check_reads(file, 0, data) && ok;
    archive = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_VIRTUAL, "reads.vfs", NULL,
        GE_VFILE_OPEN_CREATE | GE_VFILE_OPEN_DIRECTORY);
    if (!archive) return 0;
    ok = write_member(archive, "member", data, sizeof(data)) && ok;
    ok = write_member(archive, "sentinel", "DO-NOT-READ", 11) && ok;
    ok = geVFile_Close(archive) && ok;
    archive = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_VIRTUAL, "reads.vfs", NULL,
        GE_VFILE_OPEN_READONLY | GE_VFILE_OPEN_DIRECTORY);
    if (!archive) return 0;
    file = geVFile_Open(archive, "member", GE_VFILE_OPEN_READONLY);
    ok = check_reads(file, 1, data) && ok;
    ok = geVFile_Close(archive) && ok;
    fprintf(stderr, "VFile reads: DOS short success, VIRTUAL over-length rejection, exact reads and EOF\n");
    return ok;
}

static int write_cases(void)
{
    unsigned char input[9001], original[9001], output[9001];
    char key[] = "12345678";
    int mode, i, ok = 1;
    for (i=0; i<(int)sizeof(input); ++i) input[i] = (unsigned char)(i*37);
    memcpy(original, input, sizeof(input));
    for (mode=0; mode<2; ++mode) {
        geVFile *archive, *file;
        void *context = mode ? key : NULL;
        archive = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_VIRTUAL, "write.vfs", context,
            GE_VFILE_OPEN_CREATE | GE_VFILE_OPEN_DIRECTORY);
        if (!archive) return 0;
        ok = write_member(archive, "literal", "immutable", 9) && ok;
        ok = write_member(archive, "chunks", input, sizeof(input)) && ok;
        ok = !memcmp(input, original, sizeof(input)) && ok;
        ok = geVFile_Close(archive) && ok;
        archive = geVFile_OpenNewSystem(NULL, GE_VFILE_TYPE_VIRTUAL, "write.vfs", context,
            GE_VFILE_OPEN_READONLY | GE_VFILE_OPEN_DIRECTORY);
        if (!archive) return 0;
        file = geVFile_Open(archive, "literal", GE_VFILE_OPEN_READONLY);
        if (!file) { geVFile_Close(archive); return 0; }
        ok = geVFile_Read(file, output, 9) && !memcmp(output, "immutable", 9) && ok;
        ok = geVFile_Close(file) && ok;
        file = geVFile_Open(archive, "chunks", GE_VFILE_OPEN_READONLY);
        if (!file) { geVFile_Close(archive); return 0; }
        ok = geVFile_Read(file, output, sizeof(output)) && !memcmp(output, original, sizeof(output)) && ok;
        ok = geVFile_Close(file) && ok;
        ok = geVFile_Close(archive) && ok;
    }
    fprintf(stderr, "VFile writes: literal and 9001 bytes, zero/nonzero mask\n");
    return ok;
}

int main(int argc, char **argv)
{
    char directory[] = "/tmp/genesis-vfile-XXXXXX";
    int ok;
    if (argc != 2 || !mkdtemp(directory) || chdir(directory)) return 2;
    ok = !strcmp(argv[1], "lines") ? line_cases() :
         !strcmp(argv[1], "reads") ? read_cases() :
         !strcmp(argv[1], "writes") ? write_cases() : 0;
    geVFile_CloseAPI();
    unlink("loose.txt"); unlink("lines.vfs"); unlink("write.vfs");
    unlink("loose.bin"); unlink("reads.vfs");
    chdir("/"); rmdir(directory);
    return ok ? 0 : 1;
}
