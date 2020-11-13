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
#include "xpc/xpc.h"
#include "xpc_lite_internal.h"
#include "mpack.h"

#if MPACK_EXTENSIONS
static struct xpc_lite_object *
mpack2xpc_lite_extension(int8_t type, const char *data)
{
    return NULL;
}
#endif

struct xpc_lite_object *
mpack2xpc(const mpack_node_t node)
{
	xpc_lite_object_t xotmp;
	size_t i;
	xpc_lite_u val;

	switch (mpack_node_type(node)) {
	case mpack_type_nil:
		xotmp = xpc_lite_null_create();
		break;

	case mpack_type_int:
		val.i = mpack_node_i64(node);
		xotmp = xpc_lite_int64_create(val.i);
		break;

	case mpack_type_uint:
		val.ui = mpack_node_u64(node);
		xotmp = xpc_lite_uint64_create(val.ui);
		break;

	case mpack_type_bool:
		val.b = mpack_node_bool(node);
		xotmp = xpc_lite_bool_create(val.b);
		break;

	case mpack_type_double:
		val.d = mpack_node_double(node);
		xotmp = xpc_lite_double_create(val.d);
		break;

	case mpack_type_str:
		val.str = mpack_node_cstr_alloc(node, 65536);
		xotmp = xpc_lite_string_create(val.str);
		break;

	case mpack_type_bin:
        // TODO:完善这个类型
		break;

	case mpack_type_array:
		xotmp = xpc_lite_array_create(NULL, 0);
		for (i = 0; i < mpack_node_array_length(node); i++) {
			xpc_lite_object_t item = mpack2xpc(
			    mpack_node_array_at(node, i));
			xpc_lite_array_append_value(item, xotmp);
		}
		break;

	case mpack_type_map:
		xotmp = xpc_lite_dictionary_create(NULL, NULL, 0);
		for (i = 0; i < mpack_node_map_count(node); i++) {
			char *key = mpack_node_cstr_alloc(
			    mpack_node_map_key_at(node, i), 1024);
			xpc_lite_object_t value = mpack2xpc(
			    mpack_node_map_value_at(node, i));
			xpc_lite_dictionary_set_value(xotmp, key, value);
		}
		break;
#if MPACK_EXTENSIONS
	case mpack_type_ext:
		xotmp = mpack2xpc_lite_extension(mpack_node_exttype(node),
		    mpack_node_data(node));
		break;
#endif
	default:
		xotmp = NULL;
		break;
	}

	return (xotmp);
}

void
xpc2mpack(mpack_writer_t *writer, xpc_lite_object_t obj)
{
	struct xpc_lite_object *xotmp = obj;

	switch (xotmp->xo_xpc_lite_type) {
	case _XPC_TYPE_DICTIONARY:
		mpack_start_map(writer, xpc_lite_dictionary_get_count(obj));
		xpc_lite_dictionary_apply(obj, ^(const char *k, xpc_lite_object_t v) {
		    mpack_write_cstr(writer, k);
		    xpc2mpack(writer, v);
		    return ((bool)true);
		});
		mpack_finish_map(writer);
		break;

	case _XPC_TYPE_ARRAY:
		mpack_start_array(writer, xpc_lite_array_get_count(obj));
		xpc_lite_array_apply(obj, ^(size_t index __unused, xpc_lite_object_t v) {
		    xpc2mpack(writer, v);
		    return ((bool)true);
		});
		mpack_finish_map(writer);
		break;

	case _XPC_TYPE_NULL:
		mpack_write_nil(writer);
		break;

	case _XPC_TYPE_BOOL:
		mpack_write_bool(writer, xpc_lite_bool_get_value(obj));
		break;

	case _XPC_TYPE_INT64:
		mpack_write_i64(writer, xpc_lite_int64_get_value(obj));
		break;

	case _XPC_TYPE_DOUBLE:
		mpack_write_double(writer, xpc_lite_double_get_value(obj));
        break;
	case _XPC_TYPE_UINT64:
		mpack_write_u64(writer, xpc_lite_uint64_get_value(obj));
		break;

	case _XPC_TYPE_STRING:
		mpack_write_cstr(writer, xpc_lite_string_get_string_ptr(obj));
		break;

	case _XPC_TYPE_UUID:
		break;
	}
}

xpc_lite_object_t
xpc_lite_dictionary_create(const char * const *keys, const xpc_lite_object_t *values,
    size_t count)
{
	struct xpc_lite_object *xo;
	size_t i;
	xpc_lite_u val;

	xo = _xpc_lite_prim_create(_XPC_TYPE_DICTIONARY, val, count);
	
	for (i = 0; i < count; i++)
		xpc_lite_dictionary_set_value(xo, keys[i], values[i]);
	
	return (xo);
}

xpc_lite_object_t
xpc_lite_dictionary_create_reply(xpc_lite_object_t original)
{
	struct xpc_lite_object *xo_orig;

	xo_orig = original;
	if ((xo_orig->xo_flags & _XPC_FROM_WIRE) == 0)
		return (NULL);

	return xpc_lite_dictionary_create(NULL, NULL, 0);
}

#ifdef MACH
void
xpc_lite_dictionary_get_audit_token(xpc_lite_object_t xdict, audit_token_t *token)
{
	struct xpc_lite_object *xo;

	xo = xdict;
	if (xo->xo_audit_token != NULL)
		memcpy(token, xo->xo_audit_token, sizeof(*token));
}

void
xpc_lite_dictionary_set_mach_recv(xpc_lite_object_t xdict, const char *key,
    mach_port_t port)
{
	struct xpc_lite_object *xo = xdict;
	struct xpc_lite_object *xotmp;
	xpc_lite_u val;

	val.port = port;
	xotmp = _xpc_lite_prim_create(_XPC_TYPE_ENDPOINT, val, 0);

	xpc_lite_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_lite_dictionary_set_mach_send(xpc_lite_object_t xdict, const char *key,
    mach_port_t port)
{
	struct xpc_lite_object *xotmp;
	xpc_lite_u val;

	val.port = port;
	xotmp = _xpc_lite_prim_create(_XPC_TYPE_ENDPOINT, val, 0);

	xpc_lite_dictionary_set_value(xdict, key, xotmp);
}

mach_port_t
xpc_lite_dictionary_copy_mach_send(xpc_lite_object_t xdict, const char *key)
{
	struct xpc_lite_object *xo;
	const struct xpc_lite_object *xotmp;

}
#endif

void
xpc_lite_dictionary_set_value(xpc_lite_object_t xdict, const char *key,
        xpc_lite_object_t value)
{
	struct xpc_lite_object *xo;
	struct xpc_lite_dict_head *head;
	struct xpc_lite_dict_pair *pair;

	xo = xdict;
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!strcmp(pair->key, key)) {
			pair->value = value;
			return;
		}
	}

	xo->xo_size++;
	pair = malloc(sizeof(struct xpc_lite_dict_pair));
	pair->key = key;
	pair->value = value;
	TAILQ_INSERT_TAIL(&xo->xo_dict, pair, xo_link);
	xpc_lite_retain(value);
}

xpc_lite_object_t
xpc_lite_dictionary_get_value(xpc_lite_object_t xdict, const char *key)
{
	struct xpc_lite_object *xo;
	struct xpc_lite_dict_head *head;
	struct xpc_lite_dict_pair *pair;

	xo = xdict;
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!strcmp(pair->key, key))
			return (pair->value);
	}

	return (NULL);
}

size_t
xpc_lite_dictionary_get_count(xpc_lite_object_t xdict)
{
	struct xpc_lite_object *xo;

	xo = xdict;
	return (xo->xo_size);
}

void
xpc_lite_dictionary_set_bool(xpc_lite_object_t xdict, const char *key, bool value)
{;
	struct xpc_lite_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_lite_bool_create(value);
	xpc_lite_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_lite_dictionary_set_int64(xpc_lite_object_t xdict, const char *key, int64_t value)
{
	struct xpc_lite_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_lite_int64_create(value);
	xpc_lite_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_lite_dictionary_set_uint64(xpc_lite_object_t xdict, const char *key, uint64_t value)
{
	struct xpc_lite_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_lite_uint64_create(value);
	xpc_lite_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_lite_dictionary_set_double(xpc_lite_object_t xdict, const char *key, double value)
{
    struct xpc_lite_object *xo, *xotmp;

    xo = xdict;
    xotmp = xpc_lite_double_create(value);
    xpc_lite_dictionary_set_value(xdict, key, xotmp);
}

void
xpc_lite_dictionary_set_string(xpc_lite_object_t xdict, const char *key,
    const char *value)
{
	struct xpc_lite_object *xo, *xotmp;

	xo = xdict;
	xotmp = xpc_lite_string_create(value);
	xpc_lite_dictionary_set_value(xdict, key, xotmp);
}

bool
xpc_lite_dictionary_get_bool(xpc_lite_object_t xdict, const char *key)
{
	xpc_lite_object_t xo;

	xo = xpc_lite_dictionary_get_value(xdict, key);
	return (xpc_lite_bool_get_value(xo));
}

int64_t
xpc_lite_dictionary_get_int64(xpc_lite_object_t xdict, const char *key)
{
	xpc_lite_object_t xo;

	xo = xpc_lite_dictionary_get_value(xdict, key);
	return (xpc_lite_int64_get_value(xo));
}

uint64_t
xpc_lite_dictionary_get_uint64(xpc_lite_object_t xdict, const char *key)
{
	xpc_lite_object_t xo;

	xo = xpc_lite_dictionary_get_value(xdict, key);
	return (xpc_lite_uint64_get_value(xo));
}

const char *
xpc_lite_dictionary_get_string(xpc_lite_object_t xdict, const char *key)
{
	xpc_lite_object_t xo;

	xo = xpc_lite_dictionary_get_value(xdict, key);
	return (xpc_lite_string_get_string_ptr(xo));
}

double xpc_lite_dictionary_get_double(xpc_lite_object_t xdict, const char *key){
    xpc_lite_object_t xo;

    xo = xpc_lite_dictionary_get_value(xdict, key);
    return (xpc_lite_double_get_value(xo));
}

bool
xpc_lite_dictionary_apply(xpc_lite_object_t xdict, xpc_lite_dictionary_applier_t applier)
{
	struct xpc_lite_object *xo, *xotmp;
	struct xpc_lite_dict_head *head;
	struct xpc_lite_dict_pair *pair;

	xo = xdict;
	head = &xo->xo_dict;

	TAILQ_FOREACH(pair, head, xo_link) {
		if (!applier(pair->key, pair->value))
			return (false);
	}

	return (true);
}
