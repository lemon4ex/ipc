// Copyright (c) 2009-2011 Apple Inc. All rights reserved.

#ifndef __XPC_BASE_H__
#define __XPC_BASE_H__

#include <sys/cdefs.h>

#ifndef __FreeBSD__
#include <os/object.h>
#endif
#include <dispatch/dispatch.h>

#include <sys/mman.h>
#include <uuid/uuid.h>
#include <bsm/audit.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

__BEGIN_DECLS

#define XPC_EXPORT extern __attribute__((visibility("default")))
#define XPC_DECL(name) typedef struct _##name##_s * name##_t
#define XPC_GLOBAL_OBJECT(object) (&(object))


/*!
 * @typedef ipc_type_t
 * A type that describes XPC object types.
 */
typedef const struct _ipc_type_s * ipc_type_t;
#define XPC_TYPE(type) const struct _ipc_type_s type

typedef void * ipc_object_t;

typedef void (^ipc_handler_t)(ipc_object_t object);

#define XPC_TYPE_CONNECTION (&_ipc_type_connection)

XPC_EXPORT XPC_TYPE(_ipc_type_connection);
XPC_DECL(ipc_connection);

typedef void (*ipc_connection_handler_t)(ipc_connection_t connection);

#define XPC_TYPE_ENDPOINT (&_ipc_type_endpoint)

XPC_EXPORT XPC_TYPE(_ipc_type_endpoint);
XPC_DECL(ipc_endpoint);

#define XPC_TYPE_NULL (&_ipc_type_null)
XPC_EXPORT XPC_TYPE(_ipc_type_null);

#define XPC_TYPE_BOOL (&_ipc_type_bool)
XPC_EXPORT XPC_TYPE(_ipc_type_bool);

#define XPC_BOOL_TRUE XPC_GLOBAL_OBJECT(_ipc_bool_true)

XPC_EXPORT const struct _ipc_bool_s _ipc_bool_true;

#define XPC_BOOL_FALSE XPC_GLOBAL_OBJECT(_ipc_bool_false)

XPC_EXPORT const struct _ipc_bool_s _ipc_bool_false;

#define XPC_TYPE_INT64 (&_ipc_type_int64)

XPC_EXPORT XPC_TYPE(_ipc_type_int64);

#define XPC_TYPE_UINT64 (&_ipc_type_uint64)

XPC_EXPORT XPC_TYPE(_ipc_type_uint64);

#define XPC_TYPE_DOUBLE (&_ipc_type_double)

XPC_EXPORT XPC_TYPE(_ipc_type_double);

#define XPC_TYPE_DATE (&_ipc_type_date)

XPC_EXPORT XPC_TYPE(_ipc_type_date);

#define XPC_TYPE_DATA (&_ipc_type_data)

XPC_EXPORT XPC_TYPE(_ipc_type_data);

#define XPC_TYPE_STRING (&_ipc_type_string)

XPC_EXPORT XPC_TYPE(_ipc_type_string);

#define XPC_TYPE_UUID (&_ipc_type_uuid)

XPC_EXPORT XPC_TYPE(_ipc_type_uuid);

#define XPC_TYPE_FD (&_ipc_type_fd)

XPC_EXPORT XPC_TYPE(_ipc_type_fd);

/*!
 * @define XPC_TYPE_SHMEM
 * A type representing a region of shared memory.
 */
#define XPC_TYPE_SHMEM (&_ipc_type_shmem)
XPC_EXPORT XPC_TYPE(_ipc_type_shmem);

#define XPC_TYPE_ARRAY (&_ipc_type_array)
XPC_EXPORT XPC_TYPE(_ipc_type_array);

#define XPC_TYPE_DICTIONARY (&_ipc_type_dictionary)

XPC_EXPORT XPC_TYPE(_ipc_type_dictionary);

#define XPC_TYPE_ERROR (&_ipc_type_error)

XPC_EXPORT XPC_TYPE(_ipc_type_error);


//#define XPC_ERROR_KEY_DESCRIPTION _ipc_error_key_description
//
//const char *const _ipc_error_key_description;

//#define XPC_EVENT_KEY_NAME _ipc_event_key_name
//
//const char *const _ipc_event_key_name;

#pragma mark XPC Object Protocol

ipc_object_t ipc_retain(ipc_object_t object);

void ipc_release(ipc_object_t object);

ipc_type_t ipc_get_type(ipc_object_t object);

ipc_object_t ipc_copy(ipc_object_t object);

bool ipc_equal(ipc_object_t object1, ipc_object_t object2);

size_t ipc_hash(ipc_object_t object);

char * ipc_copy_description(ipc_object_t object);

#pragma mark XPC Object Types
#pragma mark Null

ipc_object_t ipc_null_create(void);

#pragma mark Boolean

ipc_object_t ipc_bool_create(bool value);

bool ipc_bool_get_value(ipc_object_t xbool);

#pragma mark Signed Integer

ipc_object_t ipc_int64_create(int64_t value);

int64_t ipc_int64_get_value(ipc_object_t xint);

#pragma mark Unsigned Integer

ipc_object_t ipc_uint64_create(uint64_t value);

uint64_t ipc_uint64_get_value(ipc_object_t xuint);

#pragma mark Double

ipc_object_t ipc_double_create(double value);

double ipc_double_get_value(ipc_object_t xdouble);

#pragma mark Date

ipc_object_t ipc_date_create(int64_t interval);

ipc_object_t ipc_date_create_from_current(void);

int64_t ipc_date_get_value(ipc_object_t xdate);

#pragma mark Data

ipc_object_t ipc_data_create(const void *bytes, size_t length);

size_t ipc_data_get_length(ipc_object_t xdata);

const void * ipc_data_get_bytes_ptr(ipc_object_t xdata);

size_t ipc_data_get_bytes(ipc_object_t xdata, void *buffer, size_t off, size_t length);

#pragma mark String

ipc_object_t ipc_string_create(const char *string);

ipc_object_t ipc_string_create_with_format(const char *fmt, ...);

ipc_object_t ipc_string_create_with_format_and_arguments(const char *fmt, va_list ap);

size_t ipc_string_get_length(ipc_object_t xstring);

const char * ipc_string_get_string_ptr(ipc_object_t xstring);

#pragma mark UUID

ipc_object_t ipc_uuid_create(const uuid_t uuid);

const uint8_t * ipc_uuid_get_bytes(ipc_object_t xuuid);

void ipc_main(ipc_connection_handler_t handler);


__END_DECLS

#endif // __XPC_BASE_H__ 