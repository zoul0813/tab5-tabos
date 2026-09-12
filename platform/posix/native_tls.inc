/* Cancellable nonblocking ESP-TLS work, executed only by the native worker. */
static int tls_connect_direct(const char* hostname, uint16_t port)
{
    const esp_tls_cfg_t configuration = {
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms        = 10000,
        .non_block         = true,
    };
    esp_tls_t* native      = esp_tls_init();
    int connected          = 0;
    const uint64_t started = platform_time_ms();
    while (native != NULL && !native_cancel_pending(&tls_cancellation) && platform_time_ms() - started < 10000U) {
        connected = esp_tls_conn_new_async(hostname, (int) strlen(hostname), (int) port, &configuration, native);
        if (connected != 0) {
            break;
        }
        vTaskDelay(1U);
    }
    if (native == NULL || connected != 1) {
        if (native != NULL) {
            (void) esp_tls_conn_destroy(native);
        }
        return tls_error();
    }
    const int result = connection_allocate(native);
    if (result < 0) {
        (void) esp_tls_conn_destroy(native);
    }
    return result;
}

static int tls_transfer_direct(native_tls_t native, tls_request_t* request, tls_response_t* response)
{
    const uint64_t started = platform_time_ms();
    while (!native_cancel_pending(&tls_cancellation)) {
        if (platform_time_ms() - started >= 10000U) {
            return tls_error();
        }
        int result;
        if (request->operation == TLS_OPERATION_SEND) {
            result = (int) esp_tls_conn_write(native, request->data, request->size);
        } else {
            result = (int) esp_tls_conn_read(native, response->data, request->size);
        }
        if (result != ESP_TLS_ERR_SSL_WANT_READ && result != ESP_TLS_ERR_SSL_WANT_WRITE) {
            if (request->operation == TLS_OPERATION_SEND && result == 0) {
                return tls_error();
            }
            return result >= 0 ? result : tls_error();
        }
        vTaskDelay(1U);
    }
    return -TABOS_ECANCELED;
}
