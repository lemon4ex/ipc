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

#include <errno.h>
#include "xpc.h"
#ifdef __APPLE__
#include <libkern/OSAtomic.h>
#define XPC_CONNECTION_NEXT_ID(conn) (OSAtomicAdd64(1,&conn->xc_last_id))
#else
#include <machine/atomic.h>
#define XPC_CONNECTION_NEXT_ID(conn) (atomic_fetchadd_int(&conn->xc_last_id,1))
#endif
#include <Block.h>
#include "xpc_lite_internal.h"

static void xpc_lite_send(xpc_lite_connection_t xconn, xpc_lite_object_t message, uint64_t id);

xpc_lite_connection_t
xpc_lite_connection_create(const char *name, dispatch_queue_t targetq)
{
	char *qname;
//	struct xpc_lite_transport *transport = xpc_lite_get_transport();
	struct xpc_lite_connection *conn;

	if ((conn = malloc(sizeof(struct xpc_lite_connection))) == NULL) {
		errno = ENOMEM;
		return (NULL);
	}

	memset(conn, 0, sizeof(struct xpc_lite_connection));
	conn->xc_last_id = 1;
	TAILQ_INIT(&conn->xc_peers);
	TAILQ_INIT(&conn->xc_pending);

	/* Create send queue */
	asprintf(&qname, "com.ixsystems.xpc.connection.sendq.%p", conn);
	conn->xc_send_queue = dispatch_queue_create(qname, NULL);

	/* Create recv queue */
	asprintf(&qname, "com.ixsystems.xpc.connection.recvq.%p", conn);
	conn->xc_recv_queue = dispatch_queue_create(qname, NULL);

    free(qname);
    
	/* Create target queue */
	conn->xc_target_queue = targetq ? targetq : dispatch_get_main_queue();

	/* Receive queue is initially suspended */
	dispatch_suspend(conn->xc_recv_queue);

	return ((xpc_lite_connection_t)conn);
}

xpc_lite_connection_t
xpc_lite_connection_create_mach_service(const char *name, dispatch_queue_t targetq,
    uint64_t flags)
{
	struct xpc_lite_transport *transport = xpc_lite_get_transport();
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xpc_lite_connection_create(name, targetq);
    if (conn == NULL){
        debugf("xpc_lite_connection_create error");
        return (NULL);
    }
    
	conn->xc_flags = flags;

	if (flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) {
		if (transport->xt_listen(name, &conn->xc_local_port) != 0) {
			debugf("Cannot create local port: %s", strerror(errno));
			return (NULL);
		}

		return ((xpc_lite_connection_t)conn);
	}

	if (transport->xt_lookup(name, &conn->xc_local_port, &conn->xc_remote_port) != 0) {
		return (NULL);
	}

	return ((xpc_lite_connection_t)conn);
}

xpc_lite_connection_t
xpc_lite_connection_create_tcp_service(const char *ip, uint16_t port, dispatch_queue_t targetq, uint64_t flags)
{
    struct xpc_lite_transport *transport = xpc_lite_get_transport();
    struct xpc_lite_connection *conn;

    conn = (struct xpc_lite_connection *)xpc_lite_connection_create(ip, targetq);
    if (conn == NULL){
        debugf("xpc_lite_connection_create error");
        return (NULL);
    }
    
    conn->xc_flags = flags;

    if (flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) {
        if (transport->xt_tcp_listen(ip, port, &conn->xc_local_port) != 0) {
            debugf("Cannot create local port: %s", strerror(errno));
            return (NULL);
        }

        return ((xpc_lite_connection_t)conn);
    }

    if (transport->xt_tcp_lookup(ip, port, &conn->xc_local_port, &conn->xc_remote_port) != 0) {
        return (NULL);
    }

    return ((xpc_lite_connection_t)conn);
}

//xpc_lite_connection_t
//xpc_lite_connection_create_from_endpoint(xpc_lite_endpoint_t endpoint)
//{
//	struct xpc_lite_connection *conn;
//
//	conn = (struct xpc_lite_connection *)xpc_lite_connection_create("anonymous", NULL);
//	if (conn == NULL)
//		return (NULL);
//
//	conn->xc_remote_port = (xpc_lite_port_t)endpoint;
//	return ((xpc_lite_connection_t)conn);
//}

void
xpc_lite_connection_set_target_queue(xpc_lite_connection_t xconn,
    dispatch_queue_t targetq)
{
	struct xpc_lite_connection *conn;

	debugf("connection=%p", xconn);
	conn = (struct xpc_lite_connection *)xconn;
	conn->xc_target_queue = targetq;	
}

void
xpc_lite_connection_set_event_handler(xpc_lite_connection_t xconn,
    xpc_lite_handler_t handler)
{
	struct xpc_lite_connection *conn;

	debugf("connection=%p", xconn);
	conn = (struct xpc_lite_connection *)xconn;
	conn->xc_handler = (xpc_lite_handler_t)Block_copy(handler);
}

void
xpc_lite_connection_suspend(xpc_lite_connection_t xconn)
{
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xconn;
	dispatch_suspend(conn->xc_recv_source);
}

void
xpc_lite_connection_resume(xpc_lite_connection_t xconn)
{
	struct xpc_lite_transport *transport = xpc_lite_get_transport();
	struct xpc_lite_connection *conn;

	debugf("connection=%p", xconn);
	conn = (struct xpc_lite_connection *)xconn;

	/* Create dispatch source for top-level connection */
	if (conn->xc_flags & XPC_CONNECTION_MACH_SERVICE_LISTENER) {
		conn->xc_recv_source = transport->xt_create_server_source(
		    conn->xc_local_port, conn, conn->xc_recv_queue);
            dispatch_resume(conn->xc_recv_source);
	} else {
		if (conn->xc_parent == NULL) {
			conn->xc_recv_source = transport->xt_create_client_source(
			    conn->xc_local_port, conn, conn->xc_recv_queue);
            dispatch_resume(conn->xc_recv_source);
		}
	}

    dispatch_resume(conn->xc_recv_queue);
}

void
xpc_lite_connection_send_message(xpc_lite_connection_t xconn,
    xpc_lite_object_t message)
{
	struct xpc_lite_connection *conn;
	uint64_t id;

	conn = (struct xpc_lite_connection *)xconn;
	id = xpc_lite_dictionary_get_uint64(message, XPC_SEQID);

    if (id == 0){
        id = XPC_CONNECTION_NEXT_ID(conn);
    }

    xpc_lite_retain(message);
	dispatch_async(conn->xc_send_queue, ^{
		xpc_lite_send(xconn, message, id);
        xpc_lite_release(message);
	});
    
}

void
xpc_lite_connection_send_message_with_reply(xpc_lite_connection_t xconn,
    xpc_lite_object_t message, dispatch_queue_t targetq, xpc_lite_handler_t handler)
{
	struct xpc_lite_connection *conn;
	struct xpc_lite_pending_call *call;

	conn = (struct xpc_lite_connection *)xconn;
	call = malloc(sizeof(struct xpc_lite_pending_call));
	call->xp_id = XPC_CONNECTION_NEXT_ID(conn);
	call->xp_handler = handler;
    call->xp_queue = targetq?:conn->xc_target_queue;
	TAILQ_INSERT_TAIL(&conn->xc_pending, call, xp_link);

    xpc_lite_retain(message);
	dispatch_async(conn->xc_send_queue, ^{
		xpc_lite_send(xconn, message, call->xp_id);
        xpc_lite_release(message);
	});

}

xpc_lite_object_t
xpc_lite_connection_send_message_with_reply_sync(xpc_lite_connection_t conn,
    xpc_lite_object_t message)
{
	__block xpc_lite_object_t result;
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);

	xpc_lite_connection_send_message_with_reply(conn, message, NULL,
	    ^(xpc_lite_object_t o) {
		result = o;
		dispatch_semaphore_signal(sem);
	});

	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
	return (result);
}

void
xpc_lite_connection_send_barrier(xpc_lite_connection_t xconn, dispatch_block_t barrier)
{
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xconn;
	dispatch_sync(conn->xc_send_queue, barrier);
}

void
xpc_lite_connection_cancel(xpc_lite_connection_t connection)
{
    struct xpc_lite_connection *conn;

    conn = (struct xpc_lite_connection *)xconn;
    dispatch_source_cancel(conn->xc_recv_source);
}

const char *
xpc_lite_connection_get_name(xpc_lite_connection_t connection)
{

	return ("unknown"); /* ??? */
}

uid_t
xpc_lite_connection_get_euid(xpc_lite_connection_t xconn)
{
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xconn;
	return (conn->xc_creds.xc_remote_euid);
}

gid_t
xpc_lite_connection_get_egid(xpc_lite_connection_t xconn)
{
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xconn;
	return (conn->xc_creds.xc_remote_guid);
}

pid_t
xpc_lite_connection_get_pid(xpc_lite_connection_t xconn)
{
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xconn;
	return (conn->xc_creds.xc_remote_pid);
}

#ifdef MACH
au_asid_t
xpc_lite_connection_get_asid(xpc_lite_connection_t xconn)
{
	struct xpc_lite_connection *conn;

	conn = xconn;
	return (conn->xc_creds.xc_remote_asid);
}
#endif

void
xpc_lite_connection_set_context(xpc_lite_connection_t xconn, void *ctx)
{
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xconn;
	conn->xc_context = ctx;
}

void *
xpc_lite_connection_get_context(xpc_lite_connection_t xconn)
{
	struct xpc_lite_connection *conn;

	conn = (struct xpc_lite_connection *)xconn;
	return (conn->xc_context);
}

void
xpc_lite_connection_set_finalizer_f(xpc_lite_connection_t connection,
    xpc_lite_finalizer_t finalizer)
{

}

xpc_lite_endpoint_t
xpc_lite_endpoint_create(xpc_lite_connection_t connection)
{
	return (NULL);
}

void
xpc_lite_main(xpc_lite_connection_handler_t handler)
{

	dispatch_main();
}

void
xpc_lite_transaction_begin(void)
{

}

void
xpc_lite_transaction_end(void)
{

}

static void
xpc_lite_send(xpc_lite_connection_t xconn, xpc_lite_object_t message, uint64_t id)
{
	struct xpc_lite_connection *conn;
    debugf("connection=%p, message=%p, id=%llu", xconn, message, id);
	conn = (struct xpc_lite_connection *)xconn;
	if (xpc_lite_pipe_send(message, id, conn->xc_local_port,
	    conn->xc_remote_port) != 0)
		debugf("send failed: %s", strerror(errno));
}

#ifdef MACH
static void
xpc_lite_connection_set_credentials(struct xpc_lite_connection *conn, audit_token_t *tok)
{
	uid_t uid;
	gid_t gid;
	pid_t pid;
	au_asid_t asid;

	if (tok == NULL)
		return;

	audit_token_to_au32(*tok, NULL, &uid, &gid, NULL, NULL, &pid, &asid,
	    NULL);

	conn->xc_creds.xc_remote_euid = uid;
	conn->xc_creds.xc_remote_guid = gid;
	conn->xc_creds.xc_remote_pid = pid;
	conn->xc_creds.xc_remote_asid = asid;
}
#endif

struct xpc_lite_connection *
xpc_lite_connection_get_peer(void *context, xpc_lite_port_t port)
{
	struct xpc_lite_transport *transport = xpc_lite_get_transport();
	struct xpc_lite_connection *conn, *peer;

	conn = context;
	TAILQ_FOREACH(peer, &conn->xc_peers, xc_link) {
		if (transport->xt_port_compare(port,
		    peer->xc_remote_port)) {
			return (peer);
		}
	}

	return (NULL);
}

void *
xpc_lite_connection_new_peer(void *context, xpc_lite_port_t local, xpc_lite_port_t remote, dispatch_source_t src)
{
//	struct xpc_lite_transport *transport = xpc_lite_get_transport();
	struct xpc_lite_connection *conn, *peer;

	conn = context;
	peer = (struct xpc_lite_connection *)xpc_lite_connection_create(NULL, NULL);
	peer->xc_parent = conn;
	peer->xc_local_port = local;
	peer->xc_remote_port = remote;
	peer->xc_recv_source = src;

	TAILQ_INSERT_TAIL(&conn->xc_peers, peer, xc_link);

	if (src) {
		dispatch_set_context(src, peer);
		dispatch_resume(src);
		dispatch_async(conn->xc_target_queue, ^{
		    conn->xc_handler(peer);
		});
	}

	return (peer);
}

void
xpc_lite_connection_destroy_peer(void *context)
{
	struct xpc_lite_connection *conn, *parent;

	conn = context;
	parent = conn->xc_parent;

	if (conn->xc_parent != NULL) {
		dispatch_async(parent->xc_target_queue, ^{
		    conn->xc_handler((xpc_lite_object_t)XPC_ERROR_CONNECTION_INVALID);
		});

		TAILQ_REMOVE(&parent->xc_peers, conn, xc_link);
	}

	dispatch_release(conn->xc_recv_source);
}

static void
xpc_lite_connection_dispatch_callback(struct xpc_lite_connection *conn,
    xpc_lite_object_t result, uint64_t id)
{
	struct xpc_lite_pending_call *call;
	TAILQ_FOREACH(call, &conn->xc_pending, xp_link) {
		if (call->xp_id == id) {
            xpc_lite_retain(result);
			dispatch_async(call->xp_queue, ^{
			    call->xp_handler(result);
                xpc_lite_release(result);
			    TAILQ_REMOVE(&conn->xc_pending, call,
				xp_link);
			    free(call);
			});
			return;
		}
	}

	if (conn->xc_handler) {
		debugf("yes");
        xpc_lite_retain(result);
		dispatch_async(conn->xc_target_queue, ^{
		    debugf("calling handler=%p", conn->xc_handler);
		    conn->xc_handler(result);
            xpc_lite_release(result);
		});
	}
}

void
xpc_lite_connection_recv_message(void *context)
{
//	struct xpc_lite_pending_call *call;
	struct xpc_lite_connection *conn;
	struct xpc_lite_credentials creds;
	xpc_lite_object_t result;
	xpc_lite_port_t remote;
	uint64_t id;
	int err;

	debugf("connection=%p", context);

	conn = context;
	err = xpc_lite_pipe_receive(conn->xc_local_port, &remote, &result, &id,
	    &creds);

    if (err < 0){
        return;
    }
    
	if (err == 0) {
		dispatch_source_cancel(conn->xc_recv_source);
		return;
	}

	debugf("msg=%p, id=%llu", result, id);

	conn->xc_creds = creds;

	xpc_lite_connection_dispatch_callback(conn, result, id);
    xpc_lite_release(result);
}

//void
//xpc_lite_connection_recv_mach_message(void *context)
//{
//	struct xpc_lite_transport *transport = xpc_lite_get_transport();
//
//	struct xpc_lite_connection *conn, *peer;
//	struct xpc_lite_credentials creds;
//	xpc_lite_object_t result;
//	xpc_lite_port_t remote;
//	uint64_t id;
//
//	debugf("connection=%p", context);
//
//	conn = context;
//	if (xpc_lite_pipe_receive(conn->xc_local_port, &remote, &result, &id,
//	    &creds) < 0)
//		return;
//
//	debugf("message=%p, id=%llu, remote=%d", result, id, remote);
//
//	peer = xpc_lite_connection_get_peer(context, remote);
//	if (!peer) {
//		debugf("new peer on port %d",remote);
//		peer = xpc_lite_connection_new_peer(context, conn->xc_local_port, remote, NULL);
//
//		dispatch_async(conn->xc_target_queue, ^{
//		    conn->xc_handler(peer);
//		    xpc_lite_connection_dispatch_callback(peer, result, id);
//		});
//	} else
//		xpc_lite_connection_dispatch_callback(peer, result, id);
//}
