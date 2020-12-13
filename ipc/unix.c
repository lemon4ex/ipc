/*
 * Copyright 2014-2015 iXsystems, Inc.
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <pthread.h>
#include <dispatch/dispatch.h>
#include "base.h"
#include "ipc_internal.h"

static int unix_lookup(const char *path, ipc_port_t *local);
static int unix_listen(const char *path, ipc_port_t *port);
static int unix_tcp_listen(const char *ip, uint16_t port, ipc_port_t *fd);
static int unix_tcp_lookup(const char *ip, uint16_t port, ipc_port_t *fd);
static int unix_release(ipc_port_t port);
static int unix_port_compare(ipc_port_t p1, ipc_port_t p2);
static dispatch_source_t unix_create_client_source(ipc_port_t port, void *, dispatch_queue_t tq);
static dispatch_source_t unix_create_server_source(ipc_port_t port, void *, dispatch_queue_t tq);
static size_t unix_send(ipc_port_t local, void *buf, size_t len);
static size_t unix_recv(ipc_port_t local, void *buf, size_t len);

static int
unix_tcp_lookup(const char *ip, uint16_t port, ipc_port_t *fd)
{
    int ret;
    struct sockaddr_in addr;
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    inet_aton(ip, &addr.sin_addr);
    addr.sin_port = htons(port);
    
    ret = socket(AF_INET, SOCK_STREAM, 0);
    
    if (connect(ret, (struct sockaddr *)&addr,sizeof(addr)) != 0) {
        debugf("connect failed: %s", strerror(errno));
        return (-1);
    }
    
    *fd = (ipc_port_t)(long)ret;
    return (0);
}

static int
unix_tcp_listen(const char *ip, uint16_t port, ipc_port_t *fd)
{
    int ret;
    struct sockaddr_in addr;
    addr.sin_len = sizeof(addr);
    addr.sin_family = AF_INET;
    inet_aton(ip, &addr.sin_addr);
    addr.sin_port = htons(port);
    
    ret = socket(AF_INET, SOCK_STREAM, 0);
    if (ret == -1){
        debugf("create socket failed: %s", strerror(errno));
        return (-1);
    }
    
    //    int one = 1;
    //    setsockopt(ret, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    
    if (bind(ret, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        debugf("bind failed: %s", strerror(errno));
        return (-1);
    }
    
    if (listen(ret, 5) != 0) {
        debugf("listen failed: %s", strerror(errno));
        return (-1);
    }
    
    *fd = (ipc_port_t)(long)ret;
    return (0);
}

static int
unix_lookup(const char *path, ipc_port_t *port)
{
    struct sockaddr_un addr;
    int ret;
    
    ret = socket(AF_UNIX, SOCK_STREAM, 0);
    addr.sun_family = AF_UNIX;
    addr.sun_len = sizeof(struct sockaddr_un);
    size_t max_path_len = sizeof(addr.sun_path);
    strncpy(addr.sun_path, path, max_path_len);
    addr.sun_path[max_path_len - 1] = '\0';
    
    if (connect(ret, (struct sockaddr *)&addr,
                addr.sun_len) != 0) {
        debugf("connect failed: %s", strerror(errno));
        return (-1);
    }
    
    *port = (ipc_port_t)(long)ret;
    return (0);
}

static int
unix_listen(const char *path, ipc_port_t *port)
{
    struct sockaddr_un addr;
    //    char *path;
    int ret;
    
    //    asprintf(&path, "%s/%s", SOCKET_DIR, name);
    
    addr.sun_family = AF_UNIX;
    addr.sun_len = sizeof(struct sockaddr_un);
    size_t max_path_len = sizeof(addr.sun_path);
    strncpy(addr.sun_path, path, max_path_len);
    addr.sun_path[max_path_len - 1] = '\0';
    
    //    free(path);
    
    unlink(addr.sun_path);
    
    ret = socket(AF_UNIX, SOCK_STREAM, 0);
    if (ret == -1){
        debugf("create socket failed: %s", strerror(errno));
        return (-1);
    }
    
    //    int one = 1;
    //    setsockopt(ret, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    
    if (bind(ret, (struct sockaddr *)&addr, sizeof(struct sockaddr_un)) != 0) {
        debugf("bind failed: %s", strerror(errno));
        return (-1);
    }
    
    if (listen(ret, 5) != 0) {
        debugf("listen failed: %s", strerror(errno));
        return (-1);
    }
    
    *port = (ipc_port_t)(long)ret;
    return (0);
}

static int
unix_release(ipc_port_t port)
{
    int fd = (int)port;
    
    if (fd != -1)
        close(fd);
    
    return (0);
}

static int
unix_port_compare(ipc_port_t p1, ipc_port_t p2)
{
    return (int)p1 == (int)p2;
}

static dispatch_source_t
unix_create_client_source(ipc_port_t port, void *context, dispatch_queue_t tq)
{
    int fd = (int)port;
    dispatch_source_t ret;
    
    ret = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
                                 (uintptr_t)fd, 0, tq);
    
    dispatch_set_context(ret, context);
    dispatch_source_set_event_handler_f(ret, ipc_connection_recv_message);
    dispatch_source_set_cancel_handler(ret, ^{
        shutdown(fd, SHUT_RDWR);
        close(fd);
        ipc_connection_destroy_peer(dispatch_get_context(ret));
    });
    
    return (ret);
}

static dispatch_source_t
unix_create_server_source(ipc_port_t port, void *context, dispatch_queue_t tq)
{
    int fd = (int)port;
    dispatch_source_t ret;
    
    ret = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ,
                                 (uintptr_t)fd, 0, tq);
    dispatch_set_context(ret, context);
    dispatch_source_set_event_handler(ret, ^{
        int sock;
        ipc_port_t client_port;
        dispatch_source_t client_source;
        
        sock = accept(fd, NULL, NULL);
        client_port = (ipc_port_t)(long)sock;
        client_source = unix_create_client_source(client_port, NULL, tq);
        ipc_connection_new_peer(context, client_port, client_source);
    });
    return (ret);
}

static size_t
unix_send(ipc_port_t local, void *buf, size_t len)
{
    int fd = (int)local;
    struct msghdr msg;
    struct iovec iov = { .iov_base = buf, .iov_len = len };
    
    debugf("local=%d, msg=%p, size=%ld", (int)local, buf, len);
    
    memset(&msg, 0, sizeof(struct msghdr));
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    if (sendmsg(fd, &msg, 0) < 0)
        return (-1);
    
    return (0);
}

static size_t
unix_recv(ipc_port_t local, void *buf, size_t len)
{
    int fd = (int)local;
    struct msghdr msg;
    
    memset(&msg, 0, sizeof(struct msghdr));
    struct iovec iov = { .iov_base = buf, .iov_len = len };
    //    int *recv_fds = NULL;
    //    size_t recv_fds_count = 0;
    ssize_t recvd;
    
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    
    recvd = recvmsg(fd, &msg, 0);
    if (recvd < 0)
        return (-1);
    
    if (recvd == 0)
        return (0);
    
    debugf("local=%d, msg=%p, len=%ld", (int)local, buf, recvd);
    
    return (recvd);
}

struct ipc_transport unix_transport = {
    .xt_name = "unix",
    .xt_listen = unix_listen,
    .xt_lookup = unix_lookup,
    .xt_tcp_listen = unix_tcp_listen,
    .xt_tcp_lookup = unix_tcp_lookup,
    .xt_release = unix_release,
    .xt_port_compare = unix_port_compare,
    .xt_create_server_source = unix_create_server_source,
    .xt_create_client_source = unix_create_client_source,
    .xt_send = unix_send,
    .xt_recv = unix_recv
};
