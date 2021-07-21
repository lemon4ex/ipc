//
//  ipc_connection.h
//  ipc
//
//  Created by h4ck on 2020/11/15.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#include <errno.h>
#ifdef __APPLE__
#include <libkern/OSAtomic.h>
#define IPC_CONNECTION_NEXT_ID(conn) (OSAtomicAdd64(1,(OSAtomic_int64_aligned64_t *)&conn->xc_last_id))
#else
#include <machine/atomic.h>
#define IPC_CONNECTION_NEXT_ID(conn) (atomic_fetchadd_int(&conn->xc_last_id,1))
#endif
#include <Block.h>
#include "base.h"
#include "ipc_internal.h"
#include "ipc_connection.h"
#include "ipc_array.h"
#include "ipc_dictionary.h"
#include "unix.h"

static void ipc_send(ipc_connection_t xconn, ipc_object_t message, uint64_t id);
static void ipc_connection_dispatch_callback(struct ipc_connection *conn, ipc_object_t result, uint64_t id);

ipc_connection_t
ipc_connection_create(dispatch_queue_t targetq)
{
	char *qname;
	struct ipc_connection *conn;

	if ((conn = malloc(sizeof(struct ipc_connection))) == NULL)
	{
		errno = ENOMEM;
		return (NULL);
	}

	memset(conn, 0, sizeof(struct ipc_connection));
	conn->xc_last_id = 1;
	TAILQ_INIT(&conn->xc_peers);
	TAILQ_INIT(&conn->xc_pending);

	asprintf(&qname, "net.ymlab.ipc.connection.sendq.%p", conn);
	conn->xc_send_queue = dispatch_queue_create(qname, NULL);

	asprintf(&qname, "net.ymlab.ipc.connection.recvq.%p", conn);
	conn->xc_recv_queue = dispatch_queue_create(qname, NULL);

	free(qname);

	conn->xc_target_queue = targetq ? targetq : dispatch_get_main_queue();

	dispatch_suspend(conn->xc_recv_queue);

	return ((ipc_connection_t)conn);
}

ipc_connection_t ipc_connection_create_uds_service(const char *path, dispatch_queue_t targetq, uint64_t flags)
{
	struct ipc_connection *conn = (struct ipc_connection *)ipc_connection_create(targetq);
	if (conn == NULL)
	{
		debugf("ipc_connection_create error");
		return (NULL);
	}

	conn->xc_flags = flags;

	if (flags & IPC_CONNECTION_LISTENER)
	{
		if (unix_listen(path, &conn->xc_local_port) != 0)
		{
			debugf("Cannot create local port: %s", strerror(errno));
			return (NULL);
		}

		return ((ipc_connection_t)conn);
	}

	if (unix_lookup(path, &conn->xc_local_port) != 0)
	{
		return (NULL);
	}

	return ((ipc_connection_t)conn);
}

ipc_connection_t ipc_connection_create_tcp_service(const char *ip, uint16_t port, dispatch_queue_t targetq, uint64_t flags)
{
	struct ipc_connection *conn = (struct ipc_connection *)ipc_connection_create(targetq);
	if (conn == NULL)
	{
		debugf("ipc_connection_create error");
		return (NULL);
	}

	conn->xc_flags = flags;

	if (flags & IPC_CONNECTION_LISTENER)
	{
		if (unix_tcp_listen(ip, port, &conn->xc_local_port) != 0)
		{
			debugf("Cannot create local port: %s", strerror(errno));
			return (NULL);
		}

		return ((ipc_connection_t)conn);
	}

	if (unix_tcp_lookup(ip, port, &conn->xc_local_port) != 0)
	{
		return (NULL);
	}

	return ((ipc_connection_t)conn);
}

void ipc_connection_set_target_queue(ipc_connection_t xconn, dispatch_queue_t targetq)
{
	struct ipc_connection *conn;

	debugf("connection=%p", xconn);
	conn = (struct ipc_connection *)xconn;
	conn->xc_target_queue = targetq;
}

void ipc_connection_set_event_handler(ipc_connection_t xconn, ipc_handler_t handler)
{
	struct ipc_connection *conn;

	debugf("connection=%p", xconn);
	conn = (struct ipc_connection *)xconn;
	conn->xc_handler = (ipc_handler_t)Block_copy(handler);
}

void ipc_connection_suspend(ipc_connection_t xconn)
{
	struct ipc_connection *conn;

	conn = (struct ipc_connection *)xconn;
	dispatch_suspend(conn->xc_recv_source);
}

void ipc_connection_resume(ipc_connection_t xconn)
{

	struct ipc_connection *conn = (struct ipc_connection *)xconn;

	debugf("connection=%p", conn);

	if (conn->xc_flags & IPC_CONNECTION_LISTENER)
	{
		conn->xc_recv_source = unix_create_server_source(conn->xc_local_port, conn, conn->xc_recv_queue);
		dispatch_resume(conn->xc_recv_source);
	}
	else
	{
		if (conn->xc_parent == NULL)
		{
			conn->xc_recv_source = unix_create_client_source(conn->xc_local_port, conn, conn->xc_recv_queue);
			dispatch_resume(conn->xc_recv_source);
		}
	}

	dispatch_resume(conn->xc_recv_queue);
}

void ipc_connection_send_message(ipc_connection_t xconn, ipc_object_t message)
{

	uint64_t id = 0;

	struct ipc_connection *conn = (struct ipc_connection *)xconn;
	if (ipc_get_type(message) == IPC_TYPE_DICTIONARY)
	{
		id = ipc_dictionary_get_uint64(message, IPC_SEQID);
	}

	if (id == 0)
	{
		id = (uint64_t)IPC_CONNECTION_NEXT_ID(conn);
	}

	ipc_retain(message);
	dispatch_async(conn->xc_send_queue, ^{
	  ipc_send(xconn, message, id);
	  ipc_release(message);
	});
}

void ipc_connection_send_message_with_reply(ipc_connection_t xconn, ipc_object_t message, dispatch_queue_t targetq, ipc_handler_t handler)
{
	struct ipc_connection *conn;
	struct ipc_pending_call *call;

	conn = (struct ipc_connection *)xconn;
	call = malloc(sizeof(struct ipc_pending_call));
	call->xp_id = (uint64_t)IPC_CONNECTION_NEXT_ID(conn);
	call->xp_handler = handler;
	call->xp_queue = targetq ?: conn->xc_target_queue;
	TAILQ_INSERT_TAIL(&conn->xc_pending, call, xp_link);

	ipc_retain(message);
	dispatch_async(conn->xc_send_queue, ^{
	  ipc_send(xconn, message, call->xp_id);
	  ipc_release(message);
	});
}

ipc_object_t ipc_connection_send_message_with_reply_sync(ipc_connection_t conn, ipc_object_t message)
{
	__block ipc_object_t result;
	dispatch_semaphore_t sem = dispatch_semaphore_create(0);

	ipc_connection_send_message_with_reply(conn, message, NULL, ^(ipc_object_t o) {
	  result = o;
	  dispatch_semaphore_signal(sem);
	});

	dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
	return (result);
}

void ipc_connection_send_barrier(ipc_connection_t xconn, dispatch_block_t barrier)
{
	struct ipc_connection *conn;

	conn = (struct ipc_connection *)xconn;
	dispatch_sync(conn->xc_send_queue, barrier);
}

void ipc_connection_cancel(ipc_connection_t xconn)
{
	struct ipc_connection *conn;

	conn = (struct ipc_connection *)xconn;
	dispatch_source_cancel(conn->xc_recv_source);
}

void ipc_connection_set_context(ipc_connection_t xconn, void *ctx)
{
	struct ipc_connection *conn;

	conn = (struct ipc_connection *)xconn;
	conn->xc_context = ctx;
}

void *ipc_connection_get_context(ipc_connection_t xconn)
{
	struct ipc_connection *conn;

	conn = (struct ipc_connection *)xconn;
	return (conn->xc_context);
}

static void ipc_send(ipc_connection_t xconn, ipc_object_t message, uint64_t id)
{
	struct ipc_connection *conn;
	debugf("connection=%p, message=%p, id=%llu", xconn, message, id);
	conn = (struct ipc_connection *)xconn;
	if (ipc_pipe_send(message, id, conn->xc_local_port) != 0)
	{
		debugf("send failed: %s", strerror(errno));
        ipc_object_t error = ipc_error_create(IPC_ERROR_CONNECTION_INVALID);
		ipc_connection_dispatch_callback(conn, error, id);
        ipc_release(error);
	}
}

struct ipc_connection *ipc_connection_get_peer(void *context, ipc_port_t port)
{
	struct ipc_connection *conn = context;
	struct ipc_connection *peer;

	TAILQ_FOREACH(peer, &conn->xc_peers, xc_link)
	{
		if (unix_port_compare(port, peer->xc_local_port))
		{
			return (peer);
		}
	}

	return (NULL);
}

void *ipc_connection_new_peer(void *context, ipc_port_t local, dispatch_source_t src)
{

	struct ipc_connection *conn = context;
	struct ipc_connection *peer = (struct ipc_connection *)ipc_connection_create(conn->xc_target_queue);
	peer->xc_parent = conn;
	peer->xc_local_port = local;
	peer->xc_recv_source = src;

	TAILQ_INSERT_TAIL(&conn->xc_peers, peer, xc_link);

	if (src)
	{
		dispatch_set_context(src, peer);
		dispatch_resume(src);
		dispatch_async(conn->xc_target_queue, ^{
		  conn->xc_handler(peer);
		});
	}

	return (peer);
}

void ipc_connection_destroy_peer(void *context)
{
	struct ipc_connection *conn, *parent;

	conn = context;
	parent = conn->xc_parent;

	if (conn->xc_parent != NULL)
	{
		dispatch_async(parent->xc_target_queue, ^{
            ipc_object_t error = ipc_error_create(IPC_ERROR_CONNECTION_INVALID);
		    conn->xc_handler(error);
            ipc_release(error);
		});

		TAILQ_REMOVE(&parent->xc_peers, conn, xc_link);
	}
	else
	{
		dispatch_async(conn->xc_target_queue, ^{
            ipc_object_t error = ipc_error_create(IPC_ERROR_CONNECTION_INVALID);
            conn->xc_handler(error);
            ipc_release(error);
		});
	}

	dispatch_release(conn->xc_recv_source);
}

static void ipc_connection_dispatch_callback(struct ipc_connection *conn, ipc_object_t result, uint64_t id)
{
	struct ipc_pending_call *call;
	TAILQ_FOREACH(call, &conn->xc_pending, xp_link)
	{
		if (call->xp_id == id)
		{
			ipc_retain(result);
			dispatch_async(call->xp_queue, ^{
			  call->xp_handler(result);
			  ipc_release(result);
			  TAILQ_REMOVE(&conn->xc_pending, call,
						   xp_link);
			  free(call);
			});
			return;
		}
	}

	if (conn->xc_handler)
	{
		ipc_retain(result);
		dispatch_async(conn->xc_target_queue, ^{
		  debugf("calling handler=%p", conn->xc_handler);
		  conn->xc_handler(result);
		  ipc_release(result);
		});
	}
}

void ipc_connection_recv_message(void *context)
{

	debugf("connection=%p", context);

	ipc_object_t result;
	uint64_t id;

	struct ipc_connection *conn = context;
	size_t err = ipc_pipe_receive(conn->xc_local_port, &result, &id);

	if (err < 0)
	{
		return;
	}

	if (err == 0)
	{
		dispatch_source_cancel(conn->xc_recv_source);
		return;
	}

	debugf("msg=%p, id=%llu", result, id);

	ipc_connection_dispatch_callback(conn, result, id);
	ipc_release(result);
}
