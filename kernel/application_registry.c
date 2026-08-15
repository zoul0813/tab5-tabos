#include <tabos/internal/application.h>

#include <string.h>

enum { APPLICATION_REGISTRY_CAPACITY = 16 };

static const tabos_app_descriptor_t *applications[APPLICATION_REGISTRY_CAPACITY];
static size_t application_count;

static bool descriptor_valid(const tabos_app_descriptor_t *descriptor)
{
    const tabos_app_capabilities_t supported = TABOS_APP_CAPABILITY_CONSOLE;
    return descriptor != NULL && descriptor->abi_version == TABOS_APPLICATION_ABI_VERSION &&
        descriptor->name != NULL && descriptor->name[0] != '\0' &&
        descriptor->version != NULL && descriptor->version[0] != '\0' &&
        descriptor->entry != NULL && (descriptor->capabilities & ~supported) == 0U;
}

bool application_registry_register(const tabos_app_descriptor_t *descriptor)
{
    if (!descriptor_valid(descriptor) || application_count >= APPLICATION_REGISTRY_CAPACITY ||
        tabos_app_find(descriptor->name) != NULL) {
        return false;
    }
    applications[application_count++] = descriptor;
    return true;
}

void application_registry_reset(void)
{
    memset(applications, 0, sizeof(applications));
    application_count = 0U;
}

size_t tabos_app_count(void)
{
    return application_count;
}

const tabos_app_descriptor_t *tabos_app_at(size_t index)
{
    return index < application_count ? applications[index] : NULL;
}

const tabos_app_descriptor_t *tabos_app_find(const char *name)
{
    if (name == NULL) {
        return NULL;
    }
    for (size_t index = 0U; index < application_count; ++index) {
        if (strcmp(applications[index]->name, name) == 0) {
            return applications[index];
        }
    }
    return NULL;
}
