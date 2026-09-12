#define _POSIX_C_SOURCE 200809L

#include "../../platform/posix/host_io.h"
#include <tabos/platform/platform.h>
#include <tabos/filesystem.h>
#include <tabos/network.h>

#include <arpa/inet.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static void check(bool condition, const char* message)
{
    if (!condition) {
        fprintf(stderr, "host network continuation failed: %s\n", message);
        exit(EXIT_FAILURE);
    }
}

static uint64_t now_ms(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t) now.tv_sec * 1000U + (uint64_t) now.tv_nsec / 1000000U;
}

uint64_t platform_time_ms(void)
{
    return now_ms();
}

void platform_log(const char* message)
{
    fprintf(stderr, "%s\n", message);
}

static void pause_briefly(void)
{
    const struct timespec delay = {.tv_nsec = 1000000};
    nanosleep(&delay, NULL);
}

static int listener(uint16_t* port)
{
    const int fd               = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_addr.s_addr = htonl(INADDR_LOOPBACK)};
    check(fd >= 0 && bind(fd, (struct sockaddr*) &address, sizeof(address)) == 0 && listen(fd, 1) == 0,
          "loopback listener");
    socklen_t size = sizeof(address);
    check(getsockname(fd, (struct sockaddr*) &address, &size) == 0, "listener address");
    *port = ntohs(address.sin_port);
    return fd;
}

static void socket_continuations(void)
{
    host_io_scope_t scope = {0};
    uint16_t port;
    const int server                         = listener(&port);
    const platform_network_address_t address = {.family = 4U, .text = "127.0.0.1"};
    const int client                         = platform_network_socket_open(4U, TABOS_SOCKET_TCP);
    host_io_enter(&scope);
    const uint64_t start = now_ms();
    int result           = platform_network_socket_accept(server, NULL, NULL);
    check(result == -TABOS_EAGAIN && scope.pending && now_ms() - start < 100U, "accept suspends");
    host_io_leave();
    host_io_cancel(&scope);
    scope = (host_io_scope_t) {0};
    do {
        host_io_enter(&scope);
        result         = platform_network_socket_connect(client, &address, port);
        scope.retrying = scope.pending;
        host_io_leave();
        pause_briefly();
    } while (scope.pending && now_ms() - start < 1000U);
    check(result == 0 && !scope.pending, "connect resumes once");
    const int accepted = accept(server, NULL, NULL);
    check(accepted >= 0, "native accept");
    char byte = 0;
    scope     = (host_io_scope_t) {0};
    host_io_enter(&scope);
    result = platform_network_socket_receive(client, &byte, 1U);
    check(result == -TABOS_EAGAIN && scope.pending, "blocking receive suspends");
    host_io_leave();
    check(send(accepted, "x", 1U, 0) == 1, "peer send");
    do {
        host_io_enter(&scope);
        result = platform_network_socket_receive(client, &byte, 1U);
        host_io_leave();
        pause_briefly();
    } while (scope.pending && now_ms() - start < 1000U);
    check(result == 1 && byte == 'x', "receive completes");
    check(platform_network_socket_set_nonblocking(client, true) == 0, "explicit nonblocking mode");
    host_io_enter(&scope);
    check(platform_network_socket_receive(client, &byte, 1U) == -TABOS_EAGAIN && !scope.pending,
          "explicit EAGAIN returns to guest");
    host_io_leave();
    check(platform_network_socket_close(client) == 0, "close client");
    close(accepted);
    close(server);
    platform_network_socket_operations_shutdown();
}

static SSL_CTX* server_context;
static atomic_bool release_server;
static atomic_bool server_ready;
static atomic_bool reject_verified_connection;
static atomic_int trust_store_failures;
static atomic_uint trust_store_calls;

int tabos_test_tls_set_default_verify_paths(SSL_CTX* context)
{
    atomic_fetch_add(&trust_store_calls, 1U);
    if (atomic_load(&trust_store_failures) > 0) {
        atomic_fetch_sub(&trust_store_failures, 1);
        return 0;
    }
    return SSL_CTX_set_default_verify_paths(context);
}

long tabos_test_tls_get_verify_result(const SSL* native)
{
    if (atomic_load(&reject_verified_connection)) {
        return X509_V_ERR_CERT_REJECTED;
    }
    return SSL_get_verify_result(native);
}

static void* tls_server(void* argument)
{
    const int fd = accept(*(int*) argument, NULL, NULL);
    check(fd >= 0, "TLS peer accepted");
    SSL* ssl = SSL_new(server_context);
    check(ssl != NULL && SSL_set_fd(ssl, fd) == 1 && SSL_accept(ssl) == 1, "TLS server handshake");
    atomic_store(&server_ready, true);
    if (atomic_load(&reject_verified_connection)) {
        SSL_free(ssl);
        close(fd);
        return NULL;
    }
    while (!atomic_load(&release_server)) {
        pause_briefly();
    }
    check(SSL_write(ssl, "z", 1) == 1, "TLS peer write");
    char byte;
    check(SSL_read(ssl, &byte, 1) == 1 && byte == 'q', "TLS peer read");
    SSL_free(ssl);
    close(fd);
    return NULL;
}

static int tls_connect_complete(const char* hostname, uint16_t port)
{
    host_io_scope_t scope = {0};
    const uint64_t start  = now_ms();
    int connection;
    do {
        host_io_enter(&scope);
        connection = platform_tls_connect(hostname, port);
        host_io_leave();
        pause_briefly();
    } while (scope.pending && now_ms() - start < 5000U);
    check(!scope.pending, "TLS worker completes before timeout");
    return connection;
}

static void tls_continuations(void)
{
    char certificate_path[] = "/tmp/tabos-tls-wait-XXXXXX";
    const int file          = mkstemp(certificate_path);
    check(file >= 0, "temporary CA");
    EVP_PKEY_CTX* key_context = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    EVP_PKEY* key             = NULL;
    check(key_context != NULL && EVP_PKEY_keygen_init(key_context) == 1 &&
              EVP_PKEY_CTX_set_ec_paramgen_curve_nid(key_context, NID_X9_62_prime256v1) == 1 &&
              EVP_PKEY_keygen(key_context, &key) == 1,
          "ephemeral TLS key");
    EVP_PKEY_CTX_free(key_context);
    X509* certificate = X509_new();
    check(certificate != NULL, "TLS certificate");
    X509_set_version(certificate, 2L);
    ASN1_INTEGER_set(X509_get_serialNumber(certificate), 1L);
    X509_gmtime_adj(X509_getm_notBefore(certificate), -60L);
    X509_gmtime_adj(X509_getm_notAfter(certificate), 3600L);
    X509_set_pubkey(certificate, key);
    X509_NAME* name = X509_get_subject_name(certificate);
    X509_NAME_add_entry_by_txt(name, "CN", MBSTRING_ASC, (const unsigned char*) "localhost", -1, -1, 0);
    X509_set_issuer_name(certificate, name);
    X509_EXTENSION* san = X509V3_EXT_conf_nid(NULL, NULL, NID_subject_alt_name, "DNS:localhost");
    check(san != NULL && X509_add_ext(certificate, san, -1) == 1 && X509_sign(certificate, key, EVP_sha256()) > 0,
          "sign TLS certificate");
    X509_EXTENSION_free(san);
    FILE* output = fdopen(file, "w");
    check(output != NULL && PEM_write_X509(output, certificate) == 1 && fclose(output) == 0, "write trusted CA");
    check(setenv("SSL_CERT_FILE", certificate_path, 1) == 0, "trust test CA");
    server_context = SSL_CTX_new(TLS_server_method());
    check(server_context != NULL && SSL_CTX_use_certificate(server_context, certificate) == 1 &&
              SSL_CTX_use_PrivateKey(server_context, key) == 1,
          "TLS server identity");
    X509_free(certificate);
    EVP_PKEY_free(key);

    atomic_store(&trust_store_failures, 2);
    check(tls_connect_complete("localhost", 443U) == -TABOS_EIO, "first trust-store failure closes context");
    check(tls_connect_complete("localhost", 443U) == -TABOS_EIO, "repeated trust-store failure retries setup");
    check(atomic_load(&trust_store_calls) == 2U, "failed trust store is not retained");

    uint16_t port;
    int server = listener(&port);
    pthread_t thread;
    atomic_store(&server_ready, false);
    atomic_store(&reject_verified_connection, true);
    check(pthread_create(&thread, NULL, tls_server, (void*) &server) == 0, "start TLS peer");
    check(tls_connect_complete("localhost", port) == -TABOS_EIO, "verification result rejects connection");
    pthread_join(thread, NULL);
    close(server);

    server = listener(&port);
    atomic_store(&server_ready, false);
    atomic_store(&release_server, false);
    atomic_store(&reject_verified_connection, false);
    check(pthread_create(&thread, NULL, tls_server, (void*) &server) == 0, "restart TLS peer");
    const int connection = tls_connect_complete("localhost", port);
    check(connection > 0, "TLS worker completes verified connection");
    while (!atomic_load(&server_ready)) {
        pause_briefly();
    }
    char byte = 0;
    host_io_scope_t scope = {0};
    const uint64_t start  = now_ms();
    host_io_enter(&scope);
    check(platform_tls_receive(connection, &byte, 1U) == -TABOS_EAGAIN && scope.pending, "TLS read suspends");
    host_io_leave();
    atomic_store(&release_server, true);
    int received;
    do {
        host_io_enter(&scope);
        received = platform_tls_receive(connection, &byte, 1U);
        host_io_leave();
        pause_briefly();
    } while (scope.pending && now_ms() - start < 5000U);
    check(received == 1 && byte == 'z', "TLS read resumes");
    int sent;
    do {
        host_io_enter(&scope);
        sent = platform_tls_send(connection, "q", 1U);
        host_io_leave();
        pause_briefly();
    } while (scope.pending && now_ms() - start < 5000U);
    check(sent == 1, "TLS send completes");
    pthread_join(thread, NULL);
    check(platform_tls_close(connection) == 0, "TLS close");
    close(server);
    SSL_CTX_free(server_context);
    check(unlink(certificate_path) == 0, "remove CA");
}

int main(void)
{
    socket_continuations();
    host_io_scope_t scope = {0};
    platform_network_address_t address;
    const uint64_t start = now_ms();
    platform_network_operation_result_t result;
    do {
        host_io_enter(&scope);
        result = platform_network_resolve("localhost", 4U, &address);
        host_io_leave();
        pause_briefly();
    } while (scope.pending && now_ms() - start < 5000U);
    check(result == PLATFORM_NETWORK_OPERATION_OK && address.family == 4U, "DNS worker completes");
    tls_continuations();
    return EXIT_SUCCESS;
}
