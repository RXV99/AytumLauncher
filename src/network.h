#ifndef NETWORK_H
#define NETWORK_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* HTTP response */
typedef struct {
    int    status_code;
    char   status_text[64];
    char   *headers;
    uint8_t *body;
    size_t body_size;
} http_response;

/* Initialize networking (SceNet) */
int  network_init(void);
void network_shutdown(void);

/* Perform HTTP GET request */
http_response *network_http_get(const char *url);

/* Perform HTTP POST request */
http_response *network_http_post(const char *url, const uint8_t *data, size_t data_size,
                                  const char *content_type);

/* Free an HTTP response */
void network_free_response(http_response *resp);

/* Simple socket connect (returns fd or < 0) */
int  network_socket_connect(const char *host, int port);

/* Socket send/receive */
int  network_socket_send(int fd, const uint8_t *data, size_t len);
int  network_socket_recv(int fd, uint8_t *buf, size_t len);
void network_socket_close(int fd);

/* Parse URL components (returns malloc'd strings) */
int  network_parse_url(const char *url, char **host, int *port, char **path);

#ifdef __cplusplus
}
#endif

#endif /* NETWORK_H */
