//
//  ipc_connection.h
//  xpc_lite
//
//  Created by h4ck on 2020/11/15.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#ifndef ipc_connection_h
#define ipc_connection_h

#include <ipc/base.h>

__BEGIN_DECLS

#define IPC_ERROR_CONNECTION_INTERRUPTED \
    IPC_GLOBAL_OBJECT(_ipc_error_connection_interrupted)
IPC_EXPORT const struct _ipc_dictionary_s _ipc_error_connection_interrupted;

#define IPC_ERROR_CONNECTION_INVALID \
    IPC_GLOBAL_OBJECT(_ipc_error_connection_invalid)
IPC_EXPORT const struct ipc_object _ipc_error_connection_invalid;

#define IPC_ERROR_TERMINATION_IMMINENT \
    IPC_GLOBAL_OBJECT(_ipc_error_termination_imminent)
IPC_EXPORT const struct _ipc_dictionary_s _ipc_error_termination_imminent;

#define IPC_CONNECTION_MACH_SERVICE_LISTENER (1 << 0)

typedef void (*ipc_finalizer_t)(void *value);

ipc_connection_t ipc_connection_create(dispatch_queue_t targetq);

ipc_connection_t ipc_connection_create_uds_service(const char *path, dispatch_queue_t targetq, uint64_t flags);

ipc_connection_t ipc_connection_create_tcp_service(const char *ip, uint16_t port, dispatch_queue_t targetq, uint64_t flags);

void ipc_connection_set_target_queue(ipc_connection_t connection, dispatch_queue_t targetq);

void ipc_connection_set_event_handler(ipc_connection_t connection, ipc_handler_t handler);

void ipc_connection_suspend(ipc_connection_t connection);

void ipc_connection_resume(ipc_connection_t connection);

void ipc_connection_send_message(ipc_connection_t connection, ipc_object_t message);

void ipc_connection_send_barrier(ipc_connection_t connection, dispatch_block_t barrier);

void ipc_connection_send_message_with_reply(ipc_connection_t connection, ipc_object_t message, dispatch_queue_t replyq, ipc_handler_t handler);

ipc_object_t ipc_connection_send_message_with_reply_sync(ipc_connection_t connection, ipc_object_t message);

void ipc_connection_cancel(ipc_connection_t connection);

void ipc_connection_set_context(ipc_connection_t connection, void *context);

void * ipc_connection_get_context(ipc_connection_t connection);

void ipc_connection_set_finalizer_f(ipc_connection_t connection, ipc_finalizer_t finalizer);

__END_DECLS

#endif /* ipc_connection_h */
