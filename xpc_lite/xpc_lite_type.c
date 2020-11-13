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

#include <sys/types.h>
#include <time.h>
#include "xpc/xpc.h"
#include "xpc_lite_internal.h"

struct _xpc_lite_type_s {
};

typedef const struct _xpc_lite_type_s xt;
xt _xpc_lite_type_array;
xt _xpc_lite_type_bool;
xt _xpc_lite_type_connection;
xt _xpc_lite_type_data;
xt _xpc_lite_type_date;
xt _xpc_lite_type_dictionary;
xt _xpc_lite_type_endpoint;
xt _xpc_lite_type_null;
xt _xpc_lite_type_error;
xt _xpc_lite_type_fd;
xt _xpc_lite_type_int64;
xt _xpc_lite_type_uint64;
xt _xpc_lite_type_shmem;
xt _xpc_lite_type_string;
xt _xpc_lite_type_uuid;
xt _xpc_lite_type_double;


struct _xpc_lite_bool_s {
};

typedef const struct _xpc_lite_bool_s xb;

xb _xpc_lite_bool_true;
xb _xpc_lite_bool_false;

struct _xpc_lite_dictionary_s {
};

typedef const struct _xpc_lite_dictionary_s xs;
xs _xpc_lite_error_connection_interrupted;
xs _xpc_lite_error_connection_invalid;
xs _xpc_lite_error_connection_imminent;
xs _xpc_lite_error_termination_imminent;

static size_t xpc_lite_data_hash(const uint8_t *data, size_t length);

static xpc_lite_type_t xpc_lite_typemap[] = {
	NULL,
	XPC_TYPE_DICTIONARY,
	XPC_TYPE_ARRAY,
	XPC_TYPE_BOOL,
	XPC_TYPE_CONNECTION,
	XPC_TYPE_ENDPOINT,
	XPC_TYPE_NULL,
	NULL,
	XPC_TYPE_INT64,
	XPC_TYPE_UINT64,
	XPC_TYPE_DATE,
	XPC_TYPE_DATA,
	XPC_TYPE_STRING,
	XPC_TYPE_UUID,
	XPC_TYPE_FD,
	XPC_TYPE_SHMEM,
	XPC_TYPE_ERROR,
	XPC_TYPE_DOUBLE
};

static const char *xpc_lite_typestr[] = {
	"invalid",
	"dictionary",
	"array",
	"bool",
	"connection",
	"endpoint",
	"null",
	"invalid",
	"int64",
	"uint64",
	"date",
	"data",
	"string",
	"uuid",
	"fd",
	"shmem",
	"error",
	"double"
};

__private_extern__ struct xpc_lite_object *
_xpc_lite_prim_create(int type, xpc_lite_u value, size_t size)
{

	return (_xpc_lite_prim_create_flags(type, value, size, 0));
}

__private_extern__ struct xpc_lite_object *
_xpc_lite_prim_create_flags(int type, xpc_lite_u value, size_t size, uint16_t flags)
{
	struct xpc_lite_object *xo;

	if ((xo = malloc(sizeof(*xo))) == NULL)
		return (NULL);

	xo->xo_size = size;
	xo->xo_xpc_lite_type = type;
	xo->xo_flags = flags;
	xo->xo_u = value;
	xo->xo_refcnt = 1;
#if MACH
	xo->xo_audit_token = NULL;
#endif

	if (type == _XPC_TYPE_DICTIONARY)
		TAILQ_INIT(&xo->xo_dict);

	if (type == _XPC_TYPE_ARRAY)
		TAILQ_INIT(&xo->xo_array);

	return (xo);
}

xpc_lite_object_t
xpc_lite_null_create(void)
{
	xpc_lite_u val;
	return _xpc_lite_prim_create(_XPC_TYPE_NULL, val, 0);
}

xpc_lite_object_t
xpc_lite_bool_create(bool value)
{
	xpc_lite_u val;

	val.b = value;
	return _xpc_lite_prim_create(_XPC_TYPE_BOOL, val, 1);
}

bool
xpc_lite_bool_get_value(xpc_lite_object_t xbool)
{
	struct xpc_lite_object *xo;

	xo = xbool;
	if (xo == NULL)
		return (0);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_BOOL)
		return (xo->xo_bool);

	return (false);
}

xpc_lite_object_t
xpc_lite_int64_create(int64_t value)
{
	xpc_lite_u val;

	val.i = value;
	return _xpc_lite_prim_create(_XPC_TYPE_INT64, val, 1);
}

int64_t
xpc_lite_int64_get_value(xpc_lite_object_t xint)
{
	struct xpc_lite_object *xo;

	xo = xint;
	if (xo == NULL)
		return (0);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_INT64)
		return (xo->xo_int);

	return (0);	
}

xpc_lite_object_t
xpc_lite_uint64_create(uint64_t value)
{
	xpc_lite_u val;

	val.ui = value;
	return _xpc_lite_prim_create(_XPC_TYPE_UINT64, val, 1);
}

uint64_t
xpc_lite_uint64_get_value(xpc_lite_object_t xuint)
{
	struct xpc_lite_object *xo;

	xo = xuint;
	if (xo == NULL)
		return (0);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_UINT64)
		return (xo->xo_uint);

	return (0);
}

xpc_lite_object_t
xpc_lite_double_create(double value)
{
	xpc_lite_u val;

	val.d = value;
	return _xpc_lite_prim_create(_XPC_TYPE_DOUBLE, val, 1);
}

double
xpc_lite_double_get_value(xpc_lite_object_t xdouble)
{
	struct xpc_lite_object *xo = xdouble;

	if (xo->xo_xpc_lite_type == _XPC_TYPE_DOUBLE)
		return (xo->xo_d);

	return (0);	
}

xpc_lite_object_t
xpc_lite_date_create(int64_t interval)
{
	xpc_lite_u val;

	val.i = interval;
	return _xpc_lite_prim_create(_XPC_TYPE_DATE, val, 1);
}

xpc_lite_object_t
xpc_lite_date_create_from_current(void)
{
	xpc_lite_u val;
	struct timespec tp;

	clock_gettime(CLOCK_REALTIME, &tp);

	val.ui = *(uint64_t *)&tp;
	return _xpc_lite_prim_create(_XPC_TYPE_DATE, val, 1);
}

int64_t
xpc_lite_date_get_value(xpc_lite_object_t xdate)
{
	struct xpc_lite_object *xo = xdate;

	if (xo == NULL)
		return (0);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_DATE)
		return (xo->xo_int);

	return (0);	
}

xpc_lite_object_t
xpc_lite_data_create(const void *bytes, size_t length)
{
	xpc_lite_u val;

	val.ptr = (uintptr_t)bytes;
	return _xpc_lite_prim_create(_XPC_TYPE_DATA, val, length);
}

#ifdef MACH
xpc_lite_object_t
xpc_lite_data_create_with_dispatch_data(dispatch_data_t ddata)
{

}
#endif

size_t
xpc_lite_data_get_length(xpc_lite_object_t xdata)
{
	struct xpc_lite_object *xo = xdata;

	if (xo == NULL)
		return (0);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_DATA)
		return (xo->xo_size);

	return (0);	
}

const void *
xpc_lite_data_get_bytes_ptr(xpc_lite_object_t xdata)
{
	struct xpc_lite_object *xo = xdata;

	if (xo == NULL)
		return (NULL);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_DATA)
		return ((const void *)xo->xo_ptr);

	return (0);	
}

size_t
xpc_lite_data_get_bytes(xpc_lite_object_t xdata, void *buffer, size_t off, size_t length)
{

	/* XXX */
	return (0);
}

xpc_lite_object_t
xpc_lite_string_create(const char *string)
{
	xpc_lite_u val;

	val.str = __DECONST(char *, string);
	return _xpc_lite_prim_create(_XPC_TYPE_STRING, val, strlen(string));
}

xpc_lite_object_t
xpc_lite_string_create_with_format(const char *fmt, ...)
{
	va_list ap;
	xpc_lite_u val;

	va_start(ap, fmt);
	vasprintf(&val.str, fmt, ap);
	va_end(ap);
	return _xpc_lite_prim_create(_XPC_TYPE_STRING, val, strlen(val.str));
}

xpc_lite_object_t
xpc_lite_string_create_with_format_and_arguments(const char *fmt, va_list ap)
{
	xpc_lite_u val;

	vasprintf(&val.str, fmt, ap);
	return _xpc_lite_prim_create(_XPC_TYPE_STRING, val, strlen(val.str));
}

size_t
xpc_lite_string_get_length(xpc_lite_object_t xstring)
{
	struct xpc_lite_object *xo = xstring;

	if (xo == NULL)
		return (0);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_STRING)
		return (xo->xo_size);

	return (0);
}

const char *
xpc_lite_string_get_string_ptr(xpc_lite_object_t xstring)
{
	struct xpc_lite_object *xo = xstring;

	if (xo == NULL)
		return (NULL);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_STRING)
		return (xo->xo_str);

	return (NULL);
}

xpc_lite_object_t
xpc_lite_uuid_create(const uuid_t uuid)
{
	xpc_lite_u val;

	memcpy(val.uuid, uuid, sizeof(uuid_t));
	return _xpc_lite_prim_create(_XPC_TYPE_UUID, val, 1);
}

const uint8_t *
xpc_lite_uuid_get_bytes(xpc_lite_object_t xuuid)
{
	struct xpc_lite_object *xo;

	xo = xuuid;
	if (xo == NULL)
		return (NULL);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_UUID)
		return ((uint8_t*)&xo->xo_uuid);

	return (NULL);
}

xpc_lite_type_t
xpc_lite_get_type(xpc_lite_object_t obj)
{
	struct xpc_lite_object *xo;

	xo = obj;
	return (xpc_lite_typemap[xo->xo_xpc_lite_type]);
}

bool
xpc_lite_equal(xpc_lite_object_t x1, xpc_lite_object_t x2)
{
	struct xpc_lite_object *xo1, *xo2;

	xo1 = x1;
	xo2 = x2;

	/* FIXME */
	return (false);
}

xpc_lite_object_t
xpc_lite_copy(xpc_lite_object_t obj)
{
	struct xpc_lite_object *xo, *xotmp;
	const void *newdata;

	xo = obj;
	switch (xo->xo_xpc_lite_type) {
		case _XPC_TYPE_BOOL:
		case _XPC_TYPE_INT64:
		case _XPC_TYPE_UINT64:
		case _XPC_TYPE_DATE:
		case _XPC_TYPE_ENDPOINT:
			return _xpc_lite_prim_create(xo->xo_xpc_lite_type, xo->xo_u, 1);

		case _XPC_TYPE_STRING:
			return xpc_lite_string_create(strdup(
			    xpc_lite_string_get_string_ptr(xo)));

		case _XPC_TYPE_DATA:
			newdata = xpc_lite_data_get_bytes_ptr(obj);
			return (xpc_lite_data_create(newdata,
			    xpc_lite_data_get_length(obj)));

		case _XPC_TYPE_DICTIONARY:
			xotmp = xpc_lite_dictionary_create(NULL, NULL, 0);
			xpc_lite_dictionary_apply(obj, ^(const char *k, xpc_lite_object_t v) {
			    xpc_lite_dictionary_set_value(xotmp, k, xpc_lite_copy(v));
			    return (bool)true;
			});
			return (xotmp);

		case _XPC_TYPE_ARRAY:
			xotmp = xpc_lite_array_create(NULL, 0);
			xpc_lite_array_apply(obj, ^(size_t idx, xpc_lite_object_t v) {
			    xpc_lite_array_set_value(xotmp, idx, xpc_lite_copy(v));
			    return ((bool)true);
			});
			return (xotmp);
	}

	return (0);
}

static size_t
xpc_lite_data_hash(const uint8_t *data, size_t length)
{
    size_t hash = 5381;

    while (length--)
        hash = ((hash << 5) + hash) + data[length];

    return (hash);
}

size_t
xpc_lite_hash(xpc_lite_object_t obj)
{
	struct xpc_lite_object *xo;
	__block size_t hash = 0;

	xo = obj;
	switch (xo->xo_xpc_lite_type) {
	case _XPC_TYPE_BOOL:
	case _XPC_TYPE_INT64:
	case _XPC_TYPE_UINT64:
	case _XPC_TYPE_DATE:
	case _XPC_TYPE_ENDPOINT:
		return ((size_t)xo->xo_u.ui);

	case _XPC_TYPE_STRING:
		return (xpc_lite_data_hash(
		    (const uint8_t *)xpc_lite_string_get_string_ptr(obj),
		    xpc_lite_string_get_length(obj)));

	case _XPC_TYPE_DATA:
		return (xpc_lite_data_hash(
		    xpc_lite_data_get_bytes_ptr(obj),
		    xpc_lite_data_get_length(obj)));

	case _XPC_TYPE_DICTIONARY:
		xpc_lite_dictionary_apply(obj, ^(const char *k, xpc_lite_object_t v) {
			hash ^= xpc_lite_data_hash((const uint8_t *)k, strlen(k));
			hash ^= xpc_lite_hash(v);
			return ((bool)true);
		});
		return (hash);

	case _XPC_TYPE_ARRAY:
		xpc_lite_array_apply(obj, ^(size_t idx, xpc_lite_object_t v) {
			hash ^= xpc_lite_hash(v);
			return ((bool)true);
		});
		return (hash);
	}

	return (0);
}

__private_extern__ const char *
_xpc_lite_get_type_name(xpc_lite_object_t obj)
{
	struct xpc_lite_object *xo;

	xo = obj;
	return (xpc_lite_typestr[xo->xo_xpc_lite_type]);
}
