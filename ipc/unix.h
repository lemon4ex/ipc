//
//  unix.h
//  ipc
//
//  Created by h4ck on 2020/11/14.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#ifndef unix_h
#define unix_h

#include <ipc/base.h>

__BEGIN_DECLS

int unix_lookup(const char *path, ipc_port_t *local);

int unix_listen(const char *path, ipc_port_t *port);

int unix_tcp_listen(const char *ip, uint16_t port, ipc_port_t *fd);

int unix_tcp_lookup(const char *ip, uint16_t port, ipc_port_t *fd);

int unix_release(ipc_port_t port);

int unix_port_compare(ipc_port_t p1, ipc_port_t p2);

dispatch_source_t unix_create_client_source(ipc_port_t port, void *, dispatch_queue_t tq);

dispatch_source_t unix_create_server_source(ipc_port_t port, void *, dispatch_queue_t tq);

size_t unix_send(ipc_port_t local, void *buf, size_t len);

size_t unix_recv(ipc_port_t local, void *buf, size_t len);

__END_DECLS

#endif /* ipc_array_h */
