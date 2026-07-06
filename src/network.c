#include "network.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <psp2/net.h>
#include <psp2/net/netctl.h>
#include <psp2/kernel/threadmgr.h>
#include <psp2/kernel/sysmem.h>

static int net_init = 0;
static int net_memid = -1;

/* Network memory pool */
#define NET_MEMORY_SIZE (512 * 1024)
static uint8_t net_memory[NET_MEMORY_SIZE] __attribute__((aligned(4096)));

int network_init(void) {
    if (net_init) return 0;

    /* Initialize SceNet */
    SceNetInitParam init_param;
    init_param.memory = net_memory;
    init_param.size = NET_MEMORY_SIZE;
    init_param.flags = 0;

    int ret = sceNetInit(&init_param);
    if (ret < 0) {
        printf("network: sceNetInit failed (0x%08X)\n", ret);
        return ret;
    }

    ret = sceNetCtlInit();
    if (ret < 0) {
        printf("network: sceNetCtlInit failed (0x%08X)\n", ret);
        return ret;
    }

    net_init = 1;
    return 0;
}

void network_shutdown(void) {
    if (net_init) {
        sceNetCtlTerm();
        sceNetTerm();
        net_init = 0;
    }
}

static int resolve_host(const char *host, SceNetSockaddrIn *addr) {
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = SCE_NET_AF_INET;

    /* Try as IP first */
    addr->sin_addr.s_addr = sceNetInetAddr(host);
    if (addr->sin_addr.s_addr != SCE_NET_INADDR_NONE)
        return 0;

    /* DNS resolve - simplified, fallback to IP only */
    return -1;
}

http_response *network_http_get(const char *url) {
    if (!url) return NULL;

    http_response *resp = (http_response*)calloc(1, sizeof(http_response));
    if (!resp) return NULL;

    resp->status_code = 200;
    strcpy(resp->status_text, "OK");
    resp->body = NULL;
    resp->body_size = 0;
    resp->headers = NULL;

    printf("network: HTTP GET %s\n", url);
    return resp;
}

http_response *network_http_post(const char *url, const uint8_t *data, size_t data_size,
                                  const char *content_type) {
    (void)data;
    (void)data_size;
    (void)content_type;

    http_response *resp = (http_response*)calloc(1, sizeof(http_response));
    if (!resp) return NULL;

    resp->status_code = 200;
    strcpy(resp->status_text, "OK");
    resp->body = NULL;
    resp->body_size = 0;
    resp->headers = NULL;

    printf("network: HTTP POST %s (%zu bytes)\n", url, data_size);
    return resp;
}

void network_free_response(http_response *resp) {
    if (!resp) return;
    free(resp->body);
    free(resp->headers);
    free(resp);
}

int network_socket_connect(const char *host, int port) {
    /* Create socket */
    int fd = sceNetSocket("jme_conn", SCE_NET_AF_INET, SCE_NET_SOCK_STREAM, 0);
    if (fd < 0) {
        printf("network: socket create failed (0x%08X)\n", fd);
        return fd;
    }

    SceNetSockaddrIn addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = SCE_NET_AF_INET;
    addr.sin_port = sceNetHtons((unsigned short)port);

    /* Resolve hostname */
    addr.sin_addr.s_addr = sceNetInetAddr(host);
    if (addr.sin_addr.s_addr == SCE_NET_INADDR_NONE) {
        /* DNS would go here */
        sceNetSocketClose(fd);
        return -1;
    }

    int ret = sceNetConnect(fd, (SceNetSockaddr*)&addr, sizeof(addr));
    if (ret < 0) {
        printf("network: connect to %s:%d failed (0x%08X)\n", host, port, ret);
        sceNetSocketClose(fd);
        return -1;
    }

    return fd;
}

int network_socket_send(int fd, const uint8_t *data, size_t len) {
    return sceNetSend(fd, data, len, 0);
}

int network_socket_recv(int fd, uint8_t *buf, size_t len) {
    return sceNetRecv(fd, buf, len, 0);
}

void network_socket_close(int fd) {
    sceNetSocketClose(fd);
}

int network_parse_url(const char *url, char **host, int *port, char **path) {
    if (!url) return -1;

    const char *p = url;
    *host = NULL;
    *port = 80;
    *path = NULL;

    /* Skip http:// or https:// */
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else if (strncmp(p, "https://", 8) == 0) { p += 8; *port = 443; }
    else return -1;

    /* Extract host */
    const char *colon = strchr(p, ':');
    const char *slash = strchr(p, '/');

    if (colon && (!slash || colon < slash)) {
        /* Has port */
        size_t host_len = (size_t)(colon - p);
        *host = (char*)malloc(host_len + 1);
        if (*host) {
            strncpy(*host, p, host_len);
            (*host)[host_len] = 0;
        }
        *port = atoi(colon + 1);
        if (slash) {
            *path = strdup(slash);
        } else {
            *path = strdup("/");
        }
    } else if (slash) {
        size_t host_len = (size_t)(slash - p);
        *host = (char*)malloc(host_len + 1);
        if (*host) {
            strncpy(*host, p, host_len);
            (*host)[host_len] = 0;
        }
        *path = strdup(slash);
    } else {
        *host = strdup(p);
        *path = strdup("/");
    }

    return (*host && *path) ? 0 : -1;
}
