#include <tabos/platform/application_admission.h>

#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>

static platform_application_admission_t admission;
static atomic_bool done;
static atomic_uint operations;

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "Application admission: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static void* worker(void* unused)
{
    (void) unused;
    while (!atomic_load(&done)) {
        if (platform_application_admit(&admission)) {
            atomic_fetch_add(&operations, 1U);
            platform_application_release(&admission);
        } else {
            (void) platform_application_acknowledge(&admission);
            sched_yield();
        }
    }
    return NULL;
}

int main(void)
{
    check(platform_application_admit(&admission), "operation before freeze admitted");
    platform_application_freeze(&admission, true);
    check(!platform_application_admit(&admission), "operation after freeze denied");
    check(!platform_application_acknowledge(&admission), "active operation prevents parking");
    platform_application_release(&admission);
    check(!platform_application_parked(&admission), "zero counters alone do not acknowledge execution");
    check(platform_application_acknowledge(&admission), "safe point acknowledges drained freeze");
    platform_application_freeze(&admission, false);
    platform_application_freeze(&admission, true);
    check(!platform_application_parked(&admission), "old acknowledgement cannot satisfy new freeze");
    check(platform_application_acknowledge(&admission), "new safe point required");
    platform_application_freeze(&admission, false);

    pthread_t thread;
    check(pthread_create(&thread, NULL, worker, NULL) == 0, "worker start");
    for (unsigned int cycle = 0U; cycle < 1000U; ++cycle) {
        platform_application_freeze(&admission, true);
        while (!platform_application_parked(&admission)) {
            sched_yield();
        }
        const unsigned int before = atomic_load(&operations);
        for (unsigned int attempt = 0U; attempt < 10U; ++attempt) {
            sched_yield();
            check(!platform_application_admit(&admission), "concurrent frozen admission denied");
        }
        check(atomic_load(&operations) == before, "acknowledged worker cannot enter operations");
        platform_application_freeze(&admission, false);
    }
    atomic_store(&done, true);
    check(pthread_join(thread, NULL) == 0, "worker joined");
    check(platform_application_admit(&admission), "admission reopened");
    platform_application_release(&admission);
    return EXIT_SUCCESS;
}
