/* Native worker operations must return through the calling gate before teardown. */
static atomic_bool socket_cancel_requested;

void platform_network_socket_operations_cancel(void)
{
    atomic_store_explicit(&socket_cancel_requested, true, memory_order_release);
}

static void execute_cancellable_socket_request(const socket_request_t* request, socket_response_t* response)
{
    memset(response, 0, sizeof(*response));
    if (request->operation == SOCKET_OPERATION_CLOSE) {
        execute_socket_request(request, response);
        return;
    }
    const bool transfer =
        request->operation == SOCKET_OPERATION_ACCEPT || request->operation == SOCKET_OPERATION_CONNECT ||
        request->operation == SOCKET_OPERATION_SEND || request->operation == SOCKET_OPERATION_RECEIVE ||
        request->operation == SOCKET_OPERATION_SEND_TO || request->operation == SOCKET_OPERATION_RECEIVE_FROM;
    int flags = -1;
    if (transfer) {
        flags = fcntl(request->socket, F_GETFL, 0);
        if (flags < 0 || fcntl(request->socket, F_SETFL, flags | O_NONBLOCK) != 0) {
            response->result = socket_error();
            return;
        }
    }
    const bool blocking    = transfer && (flags & O_NONBLOCK) == 0;
    bool connecting        = false;
    const uint64_t started = platform_time_ms();
    for (;;) {
        if (atomic_load_explicit(&socket_cancel_requested, memory_order_acquire)) {
            response->result = -TABOS_ECANCELED;
            break;
        }
        if (request->operation == SOCKET_OPERATION_WAIT) {
            socket_request_t slice = *request;
            const uint64_t elapsed = platform_time_ms() - started;
            slice.timeout_ms       = 10U;
            if (request->timeout_ms != UINT32_MAX) {
                const uint32_t remaining =
                    elapsed < request->timeout_ms ? request->timeout_ms - (uint32_t) elapsed : 0U;
                if (remaining < slice.timeout_ms) {
                    slice.timeout_ms = remaining;
                }
            }
            execute_socket_request(&slice, response);
            if (response->result != 0 ||
                (request->timeout_ms != UINT32_MAX && platform_time_ms() - started >= request->timeout_ms)) {
                break;
            }
            continue;
        }
        if (connecting) {
            struct pollfd item = {.fd = request->socket, .events = POLLOUT};
            const int ready    = poll(&item, 1U, 10);
            if (ready == 0) {
                continue;
            }
            int error      = 0;
            socklen_t size = sizeof(error);
            if (ready < 0 || getsockopt(request->socket, SOL_SOCKET, SO_ERROR, &error, &size) != 0) {
                response->result = socket_error();
            } else {
                errno            = error;
                response->result = error == 0 ? 0 : socket_error();
            }
            break;
        }
        execute_socket_request(request, response);
        if (!blocking || response->result != -TABOS_EAGAIN) {
            break;
        }
        connecting = request->operation == SOCKET_OPERATION_CONNECT;
        vTaskDelay(1U);
    }
    if (flags >= 0) {
        (void) fcntl(request->socket, F_SETFL, flags);
        if (request->operation == SOCKET_OPERATION_ACCEPT && response->result >= 0 && blocking) {
            const int accepted_flags = fcntl(response->result, F_GETFL, 0);
            if (accepted_flags < 0 || fcntl(response->result, F_SETFL, accepted_flags & ~O_NONBLOCK) != 0) {
                (void) close_native_socket(response->result);
                response->result = -TABOS_EIO;
            }
        }
    }
}
