#ifndef TABOS_SESSION_H
#define TABOS_SESSION_H

#include <stdint.h>

typedef enum {
    TABOS_SESSION_BEGIN = 1,
    TABOS_SESSION_STATUS,
    TABOS_SESSION_RESUME,
    TABOS_SESSION_CHECKPOINT,
    TABOS_SESSION_ACKNOWLEDGE,
    TABOS_SESSION_FORCE_CLOSE,
    TABOS_SESSION_BLOCKER,
} tabos_session_operation_t;

/* BEGIN returns a positive transition token. STATUS returns first blocking PID
 * or zero when parked; timeout returns -ETIMEDOUT and automatically resumes.
 * BLOCKER returns the last timeout PID. FORCE_CLOSE accepts a member PID.
 * All failures are negative TabOS errors. Only the coordinator may control peers.
 * ACKNOWLEDGE returns only after resume; it closes caller audio/camera streams.
 * Clients query CHECKPOINT at bounded safe points, then acknowledge its token. */
int tabos_session_control(tabos_session_operation_t operation, uint32_t token, uint32_t pid);

#endif
