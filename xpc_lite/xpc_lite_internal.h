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

#ifndef	_LIBXPC_XPC_INTERNAL_H
#define	_LIBXPC_XPC_INTERNAL_H

#include <sys/queue.h>
#include <sys/uio.h>
#include <dispatch/dispatch.h>
#include "mpack.h"

#ifdef XPC_DEBUG
#define debugf(...) 				\
    do { 					\
    	fprintf(stderr, "%s: ", __func__);	\
    	fprintf(stderr, __VA_ARGS__);		\
    	fprintf(stderr, "\n");			\
    } while(0);
#else
#define debugf(...)
#endif

#define _XPC_TYPE_INVALID		0
#define _XPC_TYPE_DICTIONARY		1
#define _XPC_TYPE_ARRAY			2
#define _XPC_TYPE_BOOL			3
#define _XPC_TYPE_CONNECTION		4
#define _XPC_TYPE_ENDPOINT		5
#define	_XPC_TYPE_NULL			6
#define _XPC_TYPE_INT64			8
#define _XPC_TYPE_UINT64		9
#define _XPC_TYPE_DATE			10
#define _XPC_TYPE_DATA			11
#define _XPC_TYPE_STRING		12
#define _XPC_TYPE_UUID			13
#define _XPC_TYPE_FD			14
#define _XPC_TYPE_SHMEM			15
#define _XPC_TYPE_ERROR			16
#define _XPC_TYPE_DOUBLE		17
#define _XPC_TYPE_MAX			_XPC_TYPE_DOUBLE

#define	XPC_SEQID		"XPC sequence number"
#define	XPC_PROTOCOL_VERSION	1

struct xpc_lite_object;
struct xpc_lite_dict_pair;
struct xpc_lite_resource;
struct xpc_lite_credentials;

TAILQ_HEAD(xpc_lite_dict_head, xpc_lite_dict_pair);
TAILQ_HEAD(xpc_lite_array_head, xpc_lite_object);

typedef void *xpc_lite_port_t;
typedef void (*xpc_lite_transport_init_t)();
typedef int (*xpc_lite_transport_listen_t)(const char *, xpc_lite_port_t *);
typedef int (*xpc_lite_transport_lookup)(const char *, xpc_lite_port_t *, xpc_lite_port_t *);
typedef char *(*xpc_lite_transport_port_to_string)(xpc_lite_port_t);
typedef int (*xpc_lite_transport_port_compare)(xpc_lite_port_t, xpc_lite_port_t);
typedef int (*xpc_lite_transport_release)(xpc_lite_port_t);
typedef int (*xpc_lite_transport_send)(xpc_lite_port_t, xpc_lite_port_t, void *buf,
    size_t len, struct xpc_lite_resource *, size_t);
typedef int(*xpc_lite_transport_recv)(xpc_lite_port_t, xpc_lite_port_t*, void *buf,
    size_t len, struct xpc_lite_resource **, size_t *, struct xpc_lite_credentials *);
typedef dispatch_source_t (*xpc_lite_transport_create_source)(xpc_lite_port_t,
    void *, dispatch_queue_t);

typedef union {
	struct xpc_lite_dict_head dict;
	struct xpc_lite_array_head array;
	uint64_t ui;
	int64_t i;
	char *str;
	bool b;
	double d;
	uintptr_t ptr;
	int fd;
	uuid_t uuid;
#ifdef MACH
	mach_port_t port;
#endif
} xpc_lite_u;

struct xpc_lite_frame_header {
    uint64_t version;
    uint64_t id;
    uint64_t length;
    uint64_t spare[4];
};

#define _XPC_FROM_WIRE 0x1
struct xpc_lite_object {
	uint8_t			xo_xpc_lite_type;
	uint16_t		xo_flags;
	volatile uint32_t	xo_refcnt;
	size_t			xo_size;
	xpc_lite_u			xo_u;
#ifdef MACH
	audit_token_t *		xo_audit_token;
#endif
	TAILQ_ENTRY(xpc_lite_object) xo_link;
};

struct xpc_lite_dict_pair {
	char *		key;
	struct xpc_lite_object *	value;
	TAILQ_ENTRY(xpc_lite_dict_pair) xo_link;
};

struct xpc_lite_pending_call {
	uint64_t		xp_id;
	xpc_lite_object_t		xp_response;
	dispatch_queue_t	xp_queue;
	xpc_lite_handler_t		xp_handler;
	TAILQ_ENTRY(xpc_lite_pending_call) xp_link;
};

struct xpc_lite_credentials {
    uid_t			xc_remote_euid;
    gid_t			xc_remote_guid;
    pid_t			xc_remote_pid;
#ifdef MACH
    au_asid_t			xc_remote_asid;
    audit_token_t		xc_audit_token;
#endif
};

struct xpc_lite_connection {
	const char *		xc_name;
	xpc_lite_port_t		xc_local_port;
    	xpc_lite_port_t		xc_remote_port;
	xpc_lite_handler_t		xc_handler;
	dispatch_source_t	xc_recv_source;
	dispatch_queue_t	 xc_send_queue;
	dispatch_queue_t	 xc_recv_queue;
	dispatch_queue_t	 xc_target_queue;
	int			xc_suspend_count;
	int			xc_transaction_count;
	uint64_t		xc_flags;
	volatile uint64_t	xc_last_id;
	void *			xc_context;
	struct xpc_lite_connection * xc_parent;
    struct xpc_lite_credentials	xc_creds;
	TAILQ_HEAD(, xpc_lite_pending_call) xc_pending;
	TAILQ_HEAD(, xpc_lite_connection) xc_peers;
	TAILQ_ENTRY(xpc_lite_connection) xc_link;
};

struct xpc_lite_resource {
    	int			xr_type;
#define XPC_RESOURCE_FD		0x01
#define XPC_RESOURCE_SHMEM	0x02
    	union {
	    int 		xr_fd;
	};
};

struct xpc_lite_transport {
    	const char *		xt_name;
    	pthread_once_t		xt_initialized;
    	xpc_lite_transport_init_t 	xt_init;
    	xpc_lite_transport_listen_t 	xt_listen;
    	xpc_lite_transport_lookup 	xt_lookup;
//    	xpc_lite_transport_port_to_string xt_port_to_string;
    	xpc_lite_transport_port_compare xt_port_compare;
    	xpc_lite_transport_release 	xt_release;
    	xpc_lite_transport_send 	xt_send;
    	xpc_lite_transport_recv	xt_recv;
    	xpc_lite_transport_create_source xt_create_server_source;
    	xpc_lite_transport_create_source xt_create_client_source;
};

struct xpc_lite_service {
	xpc_lite_port_t		xs_pipe;
	TAILQ_HEAD(, xpc_lite_connection) xs_connections;
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

__private_extern__ struct xpc_lite_transport *xpc_lite_get_transport();
__private_extern__ void xpc_lite_set_transport(struct xpc_lite_transport *);
__private_extern__ struct xpc_lite_object *_xpc_lite_prim_create(int type, xpc_lite_u value,
    size_t size);
__private_extern__ struct xpc_lite_object *_xpc_lite_prim_create_flags(int type,
    xpc_lite_u value, size_t size, uint16_t flags);
__private_extern__ const char *_xpc_lite_get_type_name(xpc_lite_object_t obj);
__private_extern__ struct xpc_lite_object *mpack2xpc(mpack_node_t node);
__private_extern__ void xpc2mpack(mpack_writer_t *writer, xpc_lite_object_t xo);
__private_extern__ void xpc_lite_object_destroy(struct xpc_lite_object *xo);
__private_extern__ void xpc_lite_connection_recv_message(void *);
__private_extern__ void xpc_lite_connection_recv_mach_message(void *);
__private_extern__ void *xpc_lite_connection_new_peer(void *context,
    xpc_lite_port_t local, xpc_lite_port_t remote, dispatch_source_t src);
__private_extern__ void xpc_lite_connection_destroy_peer(void *context);
__private_extern__ int xpc_lite_pipe_send(xpc_lite_object_t obj, uint64_t id,
    xpc_lite_port_t local, xpc_lite_port_t remote);
__private_extern__ int xpc_lite_pipe_receive(xpc_lite_port_t local, xpc_lite_port_t *remote,
    xpc_lite_object_t *result, uint64_t *id, struct xpc_lite_credentials *creds);

#endif	/* _LIBXPC_XPC_INTERNAL_H */
