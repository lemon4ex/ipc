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
#include <sys/errno.h>
#ifdef __APPLE__
#include <libkern/OSAtomic.h>
#else
#include <sys/sbuf.h>
#include <machine/atomic.h>
#endif
#include <assert.h>
#include <syslog.h>
#include <pthread.h>
#include "xpc/xpc.h"
#include "xpc_lite_internal.h"

#define RECV_BUFFER_SIZE	65536

//static void xpc_lite_copy_description_level(xpc_lite_object_t obj, struct sbuf *sbuf,
//    int level);

extern struct xpc_lite_transport unix_transport __attribute__((weak));
extern struct xpc_lite_transport mach_transport __attribute__((weak));
static struct xpc_lite_transport *selected_transport = NULL;

struct xpc_lite_transport *
xpc_lite_get_transport()
{
	if (!selected_transport) {
		char *env = getenv("XPC_TRANSPORT");
		if (env) {
			if (!strcmp(env, "unix"))
				selected_transport = &unix_transport;
#ifdef MACH
			if (!strcmp(env, "mach"))
				selected_transport = &mach_transport;
#endif
		} else {
#ifdef MACH
			selected_transport = &mach_transport;
#else
			selected_transport = &unix_transport;
#endif
		}
	}

	return (selected_transport);
}

static void
xpc_lite_dictionary_destroy(struct xpc_lite_object *dict)
{
	struct xpc_lite_dict_head *head;
	struct xpc_lite_dict_pair *p, *ptmp;

	head = &dict->xo_dict;

	TAILQ_FOREACH_SAFE(p, head, xo_link, ptmp) {
		TAILQ_REMOVE(head, p, xo_link);
		xpc_lite_object_destroy(p->value);
		free(p);
	}
}

static void
xpc_lite_array_destroy(struct xpc_lite_object *dict)
{
	struct xpc_lite_object *p, *ptmp;
	struct xpc_lite_array_head *head;

	head = &dict->xo_array;

	TAILQ_FOREACH_SAFE(p, head, xo_link, ptmp) {
		TAILQ_REMOVE(head, p, xo_link);
		xpc_lite_object_destroy(p);
	}
}

static int
xpc_lite_pack(struct xpc_lite_object *xo, void **buf, uint64_t id, size_t *size)
{
	struct xpc_lite_frame_header *header;
	mpack_writer_t writer;
	char *packed, *ret;
	size_t packed_size;

	mpack_writer_init_growable(&writer, &packed, &packed_size);
	xpc2mpack(&writer, xo);

	if (mpack_writer_destroy(&writer) != mpack_ok)
		return (-1);

	ret = malloc(packed_size + sizeof(*header));
	memset(ret, 0, packed_size + sizeof(*header));

	header = (struct xpc_lite_frame_header *)ret;
	header->length = packed_size;
	header->id = id;
	header->version = XPC_PROTOCOL_VERSION;

	memcpy(ret + sizeof(*header), packed, packed_size);
	*buf = ret;
	*size = packed_size + sizeof(*header);

	free(packed);
	return (0);
}

static struct xpc_lite_object *
xpc_lite_unpack(void *buf, size_t size)
{
	mpack_tree_t tree;
	struct xpc_lite_object *xo;

	mpack_tree_init(&tree, (const char *)buf, size);
	if (mpack_tree_error(&tree) != mpack_ok) {
		debugf("unpack failed: %d", mpack_tree_error(&tree))
		return (NULL);
	}
    mpack_tree_parse(&tree);
	xo = mpack2xpc(mpack_tree_root(&tree));
	return (xo);
}

void
xpc_lite_object_destroy(struct xpc_lite_object *xo)
{
	if (xo->xo_xpc_lite_type == _XPC_TYPE_DICTIONARY)
		xpc_lite_dictionary_destroy(xo);

	if (xo->xo_xpc_lite_type == _XPC_TYPE_ARRAY)
		xpc_lite_array_destroy(xo);

	free(xo);
}

xpc_lite_object_t
xpc_lite_retain(xpc_lite_object_t obj)
{
	struct xpc_lite_object *xo;

	xo = obj;
#ifdef __APPLE__
    OSAtomicAdd32(1,&xo->xo_refcnt);
#else
    atomic_add_int(&xo->xo_refcnt, 1);
#endif
	
	return (obj);
}

void
xpc_lite_release(xpc_lite_object_t obj)
{
	struct xpc_lite_object *xo;

	xo = obj;
#ifdef __APPLE__
    if (OSAtomicAdd32(-1,&xo->xo_refcnt) > 0) {
        return;
    }
#else
    if (atomic_fetchadd_int(&xo->xo_refcnt, -1) > 0)
        return;
#endif
	xpc_lite_object_destroy(xo);
}

static const char *xpc_lite_errors[] = {
	"No Error Found",
	"No Memory",
	"Invalid Argument",
	"No Such Process"
};

#if 0
const char *
xpc_lite_strerror(int error)
{

	if (error > EXMAX || error < 0)
		return "BAD ERROR";
	return (xpc_lite_errors[error]);
}
#endif

char *
xpc_lite_copy_description(xpc_lite_object_t obj)
{
    
    // TODO:
//	char *result;
//	struct sbuf *sbuf;
//
//	sbuf = sbuf_new_auto();
//	xpc_lite_copy_description_level(obj, sbuf, 0);
//	sbuf_finish(sbuf);
//	result = strdup(sbuf_data(sbuf));
//	sbuf_delete(sbuf);
//
//	return (result);
    return "";
}

//static void
//xpc_lite_copy_description_level(xpc_lite_object_t obj, struct sbuf *sbuf, int level)
//{
//	struct xpc_lite_object *xo = obj;
//	struct uuid *id;
//	char *uuid_str;
//	uint32_t uuid_status;
//
//	if (obj == NULL) {
//		sbuf_printf(sbuf, "<null value>\n");
//		return;
//	}
//
//	sbuf_printf(sbuf, "(%s) ", _xpc_lite_get_type_name(obj));
//
//	switch (xo->xo_xpc_lite_type) {
//	case _XPC_TYPE_DICTIONARY:
//		sbuf_printf(sbuf, "\n");
//		xpc_lite_dictionary_apply(xo, ^(const char *k, xpc_lite_object_t v) {
//			sbuf_printf(sbuf, "%*s\"%s\": ", level * 4, " ", k);
//			xpc_lite_copy_description_level(v, sbuf, level + 1);
//			return ((bool)true);
//		});
//		break;
//
//	case _XPC_TYPE_ARRAY:
//		sbuf_printf(sbuf, "\n");
//		xpc_lite_array_apply(xo, ^(size_t idx, xpc_lite_object_t v) {
//			sbuf_printf(sbuf, "%*s%ld: ", level * 4, " ", idx);
//			xpc_lite_copy_description_level(v, sbuf, level + 1);
//			return ((bool)true);
//		});
//		break;
//
//	case _XPC_TYPE_BOOL:
//		sbuf_printf(sbuf, "%s\n",
//		    xpc_lite_bool_get_value(obj) ? "true" : "false");
//		break;
//
//	case _XPC_TYPE_STRING:
//		sbuf_printf(sbuf, "\"%s\"\n",
//		    xpc_lite_string_get_string_ptr(obj));
//		break;
//
//	case _XPC_TYPE_INT64:
//		sbuf_printf(sbuf, "%ld\n",
//		    xpc_lite_int64_get_value(obj));
//		break;
//
//	case _XPC_TYPE_UINT64:
//		sbuf_printf(sbuf, "%lx\n",
//		    xpc_lite_uint64_get_value(obj));
//		break;
//
//	case _XPC_TYPE_DATE:
//		sbuf_printf(sbuf, "%lu\n",
//		    xpc_lite_date_get_value(obj));
//		break;
//
//	case _XPC_TYPE_UUID:
//		id = (struct uuid *)xpc_lite_uuid_get_bytes(obj);
//		uuid_to_string(id, &uuid_str, &uuid_status);
//		sbuf_printf(sbuf, "%s\n", uuid_str);
//		free(uuid_str);
//		break;
//
//	case _XPC_TYPE_ENDPOINT:
//		sbuf_printf(sbuf, "<%ld>\n", xo->xo_int);
//		break;
//
//	case _XPC_TYPE_NULL:
//		sbuf_printf(sbuf, "<null>\n");
//		break;
//	}
//}

#ifdef MACH
struct _launch_data {
	uint64_t type;
	union {
		struct {
			union {
				launch_data_t *_array;
				char *string;
				void *opaque;
				int64_t __junk;
			};
			union {
				uint64_t _array_cnt;
				uint64_t string_len;
				uint64_t opaque_size;
			};
		};
		int64_t fd;
		uint64_t  mp;
		uint64_t err;
		int64_t number;
		uint64_t boolean; /* We'd use 'bool' but this struct needs to be used under Rosetta, and sizeof(bool) is different between PowerPC and Intel */
		double float_num;
	};
};

static uint8_t ld_to_xpc_lite_type[] = {
	_XPC_TYPE_INVALID,
	_XPC_TYPE_DICTIONARY,
	_XPC_TYPE_ARRAY,
	_XPC_TYPE_FD,
	_XPC_TYPE_UINT64,
	_XPC_TYPE_DOUBLE,
	_XPC_TYPE_BOOL,
	_XPC_TYPE_STRING,
	_XPC_TYPE_DATA,
	_XPC_TYPE_ERROR,
	_XPC_TYPE_ENDPOINT
};
	
xpc_lite_object_t
ld2xpc(launch_data_t ld)
{
	struct xpc_lite_object *xo;
	xpc_lite_u val;


	if (ld->type > LAUNCH_DATA_MACHPORT)
		return (NULL);
	if (ld->type == LAUNCH_DATA_STRING || ld->type == LAUNCH_DATA_OPAQUE) {
		val.str = malloc(ld->string_len);
		memcpy(__DECONST(void *, val.str), ld->string, ld->string_len);
		xo = _xpc_lite_prim_create(ld_to_xpc_lite_type[ld->type], val, ld->string_len);
	} else if (ld->type == LAUNCH_DATA_BOOL) {
		val.b = (bool)ld->boolean;
		xo = _xpc_lite_prim_create(ld_to_xpc_lite_type[ld->type], val, 0);
	} else if (ld->type == LAUNCH_DATA_ARRAY) {
		xo = xpc_lite_array_create(NULL, 0);
		for (uint64_t i = 0; i < ld->_array_cnt; i++)
			xpc_lite_array_append_value(xo, ld2xpc(ld->_array[i]));
	} else {
		val.ui = ld->mp;
		xo = _xpc_lite_prim_create(ld_to_xpc_lite_type[ld->type], val, ld->string_len);
	}
	return (xo);
}
#endif

#if 0
xpc_lite_object_t
xpc_lite_copy_entitlement_for_token(const char *key __unused, audit_token_t *token __unused)
{
	xpc_lite_u val;

	val.b = true;
	return (_xpc_lite_prim_create(_XPC_TYPE_BOOL, val,0));
}
#endif

int
xpc_lite_pipe_send(xpc_lite_object_t xobj, uint64_t id, xpc_lite_port_t local, xpc_lite_port_t remote)
{
	struct xpc_lite_transport *transport = xpc_lite_get_transport();
	void *buf;
	size_t size;

	assert(xpc_lite_get_type(xobj) == &_xpc_lite_type_dictionary);

	if (xpc_lite_pack(xobj, &buf, id, &size) != 0) {
		debugf("pack failed");
		return (-1);
	}

	if (transport->xt_send(local, remote, buf, size, NULL, 0) != 0) {
		debugf("transport send function failed: %s", strerror(errno));
		return (-1);
	}

	return (0);
}

int
xpc_lite_pipe_receive(xpc_lite_port_t local, xpc_lite_port_t *remote, xpc_lite_object_t *result,
    uint64_t *id, struct xpc_lite_credentials *creds)
{
	struct xpc_lite_transport *transport = xpc_lite_get_transport();
	struct xpc_lite_resource *resources;
	struct xpc_lite_frame_header *header;
	void *buffer;
	size_t nresources;
	int ret;

	buffer = malloc(RECV_BUFFER_SIZE);

	ret = transport->xt_recv(local, remote, buffer, RECV_BUFFER_SIZE,
	    &resources, &nresources, creds);
	if (ret < 0) {
		debugf("transport receive function failed: %s", strerror(errno));
        free(buffer);
		return (-1);
	}

	if (ret == 0) {
		debugf("remote side closed connection, port=%d", (uint32_t)local);
        free(buffer);
		return (ret);
	}

	header = (struct xpc_lite_frame_header *)buffer;
	if (header->length > (ret - sizeof(*header))) {
		debugf("invalid message length");
        free(buffer);
		return (-1);
	}

	if (header->version != XPC_PROTOCOL_VERSION) {
		debugf("invalid protocol version")
        free(buffer);
		return (-1);
	}

	*id = header->id;

	debugf("length=%lld", header->length);

	*result = xpc_lite_unpack(buffer + sizeof(*header), (size_t)header->length);

    if (*result == NULL){
        free(buffer);
        return (-1);
    }
    
	free(buffer);
	return (ret);
}
