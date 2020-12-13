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

#ifndef	_LIBIPC_IPC_INTERNAL_H
#define	_LIBIPC_IPC_INTERNAL_H

#include <sys/queue.h>
#include <sys/uio.h>
#include <dispatch/dispatch.h>
#include "mpack.h"

#ifdef IPC_DEBUG
#define debugf(...) 				\
    do { 					\
    	fprintf(stderr, "%s: ", __func__);	\
    	fprintf(stderr, __VA_ARGS__);		\
    	fprintf(stderr, "\n");			\
    } while(0);
#else
#define debugf(...)
#endif

#define _IPC_TYPE_INVALID		0
#define _IPC_TYPE_DICTIONARY		1
#define _IPC_TYPE_ARRAY			2
#define _IPC_TYPE_BOOL			3
//#define _IPC_TYPE_CONNECTION		4
//#define _IPC_TYPE_ENDPOINT		5
#define	_IPC_TYPE_NULL			6
#define _IPC_TYPE_INT64			8
#define _IPC_TYPE_UINT64		9
#define _IPC_TYPE_DATE			10
#define _IPC_TYPE_DATA			11
#define _IPC_TYPE_STRING		12
#define _IPC_TYPE_UUID			13
//#define _IPC_TYPE_FD			14
//#define _IPC_TYPE_SHMEM			15
#define _IPC_TYPE_ERROR			16
#define _IPC_TYPE_DOUBLE		17
#define _IPC_TYPE_MAX			_IPC_TYPE_DOUBLE

#define	IPC_SEQID		"XPC sequence number"
#define	IPC_PROTOCOL_VERSION	1

struct ipc_object;
struct ipc_dict_pair;

TAILQ_HEAD(ipc_dict_head, ipc_dict_pair);
TAILQ_HEAD(ipc_array_head, ipc_object);

typedef void *ipc_port_t;
typedef void (*ipc_transport_init_t)(void);
typedef int (*ipc_transport_listen_t)(const char *, ipc_port_t *);
typedef int (*ipc_transport_lookup)(const char *, ipc_port_t *);
typedef int (*ipc_transport_tcp_listen_t)(const char *, uint16_t, ipc_port_t *);
typedef int (*ipc_transport_tcp_lookup)(const char *, uint16_t, ipc_port_t *);
typedef int (*ipc_transport_port_compare)(ipc_port_t, ipc_port_t);
typedef int (*ipc_transport_release)(ipc_port_t);
typedef size_t (*ipc_transport_send)(ipc_port_t, void *buf, size_t len);
typedef size_t (*ipc_transport_recv)(ipc_port_t, void *buf, size_t len);
typedef dispatch_source_t (*ipc_transport_create_source)(ipc_port_t, void *, dispatch_queue_t);

typedef union {
	struct ipc_dict_head dict;
	struct ipc_array_head array;
	uint64_t ui;
	int64_t i;
	char *str;
	bool b;
	double d;
	uintptr_t ptr;
	int fd;
	uuid_t uuid;
} ipc_u;

struct ipc_frame_header {
    uint64_t version;
    uint64_t id;
    uint64_t length;
    uint64_t spare[4];
};

#define _IPC_FROM_WIRE 0x1
struct ipc_object {
	uint8_t			xo_ipc_type;
	uint16_t		xo_flags;
	volatile uint32_t	xo_refcnt;
	size_t			xo_size;
	ipc_u			xo_u;
	TAILQ_ENTRY(ipc_object) xo_link;
};

struct ipc_dict_pair {
	char *		key;
	struct ipc_object *	value;
	TAILQ_ENTRY(ipc_dict_pair) xo_link;
};

struct ipc_pending_call {
	uint64_t		xp_id;
	ipc_object_t		xp_response;
	dispatch_queue_t	xp_queue;
	ipc_handler_t		xp_handler;
	TAILQ_ENTRY(ipc_pending_call) xp_link;
};

struct ipc_connection {
	ipc_port_t		xc_local_port;
	ipc_handler_t		xc_handler;
	dispatch_source_t	xc_recv_source;
	dispatch_queue_t	 xc_send_queue;
	dispatch_queue_t	 xc_recv_queue;
	dispatch_queue_t	 xc_target_queue;
	int			xc_suspend_count;
	int			xc_transaction_count;
	uint64_t		xc_flags;
	volatile uint64_t	xc_last_id;
	void *			xc_context;
	struct ipc_connection * xc_parent;
	TAILQ_HEAD(, ipc_pending_call) xc_pending;
	TAILQ_HEAD(, ipc_connection) xc_peers;
	TAILQ_ENTRY(ipc_connection) xc_link;
};

struct ipc_resource {
    	int			xr_type;
#define IPC_RESOURCE_FD		0x01
#define IPC_RESOURCE_SHMEM	0x02
    	union {
	    int 		xr_fd;
	};
};

struct ipc_transport {
    	const char *		xt_name;
    	pthread_once_t		xt_initialized;
    	ipc_transport_init_t 	xt_init;
    	ipc_transport_listen_t 	xt_listen;
    	ipc_transport_lookup 	xt_lookup;
        ipc_transport_tcp_listen_t     xt_tcp_listen;
        ipc_transport_tcp_lookup     xt_tcp_lookup;
//    	ipc_transport_port_to_string xt_port_to_string;
    	ipc_transport_port_compare xt_port_compare;
    	ipc_transport_release 	xt_release;
    	ipc_transport_send 	xt_send;
    	ipc_transport_recv	xt_recv;
    	ipc_transport_create_source xt_create_server_source;
    	ipc_transport_create_source xt_create_client_source;
};

struct ipc_service {
	ipc_port_t		xs_pipe;
	TAILQ_HEAD(, ipc_connection) xs_connections;
};

#define xo_str xo_u.str
#define xo_bool xo_u.b
#define xo_uint xo_u.ui
#define xo_int xo_u.i
#define xo_ptr xo_u.ptr
#define xo_d xo_u.d
#define xo_fd xo_u.fd
#define xo_uuid xo_u.uuid
#define xo_port xo_u.port
#define xo_array xo_u.array
#define xo_dict xo_u.dict

struct ipc_transport *ipc_get_transport(void);
void ipc_set_transport(struct ipc_transport *);
struct ipc_object *_ipc_prim_create(int type, ipc_u value, size_t size);
struct ipc_object *_ipc_prim_create_flags(int type, ipc_u value, size_t size, uint16_t flags);
const char *_ipc_get_type_name(ipc_object_t obj);
struct ipc_object *mpack2xpc(mpack_node_t node);
void xpc2mpack(mpack_writer_t *writer, ipc_object_t xo);
void ipc_object_destroy(struct ipc_object *xo);
void ipc_connection_recv_message(void *);
void *ipc_connection_new_peer(void *context, ipc_port_t local, dispatch_source_t src);
void ipc_connection_destroy_peer(void *context);
int ipc_pipe_send(ipc_object_t obj, uint64_t id, ipc_port_t local);
size_t ipc_pipe_receive(ipc_port_t local, ipc_object_t *result, uint64_t *id);

#endif	/* _LIBIPC_IPC_INTERNAL_H */
