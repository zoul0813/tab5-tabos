#include <starfall/storage.h>

#include <errno.h>
#include <stdbool.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define SCORE_DIRECTORY "T:/data/starfall"
#define SCORE_PATH SCORE_DIRECTORY "/highscore.dat"
#define SCORE_TEMP_PATH SCORE_DIRECTORY "/highscore.tmp"
#define SCORE_MAX 999999U

uint32_t starfall_high_score_load(void)
{
    const int descriptor = open(SCORE_PATH, O_RDONLY);
    if (descriptor < 0) return 0U;
    char buffer[24];
    const ssize_t count = read(descriptor, buffer, sizeof(buffer) - 1U);
    (void)close(descriptor);
    if (count <= 0) return 0U;
    buffer[count] = '\0';
    char *end = NULL;
    errno = 0;
    const unsigned long value = strtoul(buffer, &end, 10);
    if (errno != 0 || end == buffer || (*end != '\n' && *end != '\0') || value > SCORE_MAX)
        return 0U;
    return (uint32_t)value;
}

int starfall_high_score_save(uint32_t score)
{
    if (score > SCORE_MAX) score = SCORE_MAX;
    if (mkdir("T:/data", 0777) != 0 && errno != EEXIST) return -1;
    if (mkdir(SCORE_DIRECTORY, 0777) != 0 && errno != EEXIST) return -1;
    const int descriptor = open(SCORE_TEMP_PATH, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (descriptor < 0) return -1;
    char buffer[24];
    const int length = snprintf(buffer, sizeof(buffer), "%lu\n", (unsigned long)score);
    const bool written = length > 0 && write(descriptor, buffer, (size_t)length) == length;
    const bool closed = close(descriptor) == 0;
    if (!written || !closed) { (void)unlink(SCORE_TEMP_PATH); return -1; }
    if (rename(SCORE_TEMP_PATH, SCORE_PATH) != 0) {
        (void)unlink(SCORE_PATH);
        if (rename(SCORE_TEMP_PATH, SCORE_PATH) != 0) {
            (void)unlink(SCORE_TEMP_PATH);
            return -1;
        }
    }
    return 0;
}
