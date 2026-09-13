#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <starfall/storage.h>

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static bool fail_rename;

int test_starfall_rename(const char* old_path, const char* new_path);

int test_starfall_rename(const char* old_path, const char* new_path)
{
    if (fail_rename) {
        errno = EIO;
        return -1;
    }
    return rename(old_path, new_path);
}

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "starfall storage test failed: %s (errno %d)\n", message, errno);
        exit(EXIT_FAILURE);
    }
}

int main(void)
{
    char original_directory[4096];
    check(getcwd(original_directory, sizeof(original_directory)) != NULL, "original directory");
    char root[] = "/tmp/tabos-starfall-storage-XXXXXX";
    check(mkdtemp(root) != NULL && chdir(root) == 0, "temporary root");
    check(mkdir("T:", 0700) == 0 && mkdir("T:/data", 0700) == 0, "drive fixture");

    check(starfall_high_score_save(123U) == 0 && starfall_high_score_load() == 123U, "initial score");
    check(starfall_high_score_save(456U) == 0 && starfall_high_score_load() == 456U, "replace score");
    fail_rename = true;
    check(starfall_high_score_save(789U) == -1 && errno == EIO, "report rename failure");
    check(starfall_high_score_load() == 456U, "preserve previous score");
    check(access("T:/data/starfall/highscore.tmp", F_OK) != 0, "remove failed temporary");

    check(unlink("T:/data/starfall/highscore.dat") == 0, "remove score");
    check(rmdir("T:/data/starfall") == 0 && rmdir("T:/data") == 0 && rmdir("T:") == 0, "remove fixture directories");
    check(chdir(original_directory) == 0 && rmdir(root) == 0, "remove temporary root");
    return EXIT_SUCCESS;
}
