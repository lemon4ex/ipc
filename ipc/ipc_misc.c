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
#include "sbuf.h"
#else
#include <sys/sbuf.h>
#include <machine/atomic.h>
#endif
#include <assert.h>
#include <syslog.h>
#include <pthread.h>
#include "base.h"
#include "ipc_internal.h"
#include "ipc_array.h"
#include "ipc_dictionary.h"

#define RECV_BUFFER_SIZE	65536

static void ipc_copy_description_level(ipc_object_t obj, struct sbuf *sbuf, int level);

extern struct ipc_transport unix_transport __attribute__((weak));
extern struct ipc_transport mach_transport __attribute__((weak));

#ifdef __APPLE__
// https://github.com/AlexShiLucky/nuttx-kernel/blob/ec83dc2ad36c278248b260bade84768352362a15/include/uuid.h
#define uuid_s_ok                       0
#define uuid_s_bad_version              1
#define uuid_s_invalid_string_uuid      2
#define uuid_s_no_memory                3

/* Length of a node address (an IEEE 802 address). */

#define UUID_NODE_LEN                   6

/* Length of a UUID. */

#define UUID_STR_LEN                    36

struct uuid
{
    uint32_t time_low;
    uint16_t time_mid;
    uint16_t time_hi_and_version;
    uint8_t  clock_seq_hi_and_reserved;
    uint8_t  clock_seq_low;
    uint8_t  node[UUID_NODE_LEN];
};
void uuid_to_string(const struct uuid *u, char **s, uint32_t *status)
{
    static const struct uuid nil = { .time_low = 0 };
    
    if (status != NULL)
        *status = uuid_s_ok;
    
    /* Why allow a NULL-pointer here? */
    if (s == 0)
        return;
    
    if (u == NULL)
        u = &nil;
    
    asprintf(s, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             u->time_low, u->time_mid, u->time_hi_and_version,
             u->clock_seq_hi_and_reserved, u->clock_seq_low, u->node[0],
             u->node[1], u->node[2], u->node[3], u->node[4], u->node[5]);
    
    if (*s == NULL && status != NULL)
        *status = uuid_s_no_memory;
}
#endif

struct ipc_transport *
ipc_get_transport()
{
    return (&unix_transport);
}

static void
ipc_dictionary_destroy(struct ipc_object *dict)
{
    struct ipc_dict_head *head;
    struct ipc_dict_pair *p, *ptmp;
    
    head = &dict->xo_dict;
    
    TAILQ_FOREACH_SAFE(p, head, xo_link, ptmp) {
        TAILQ_REMOVE(head, p, xo_link);
        free(p->key);
        ipc_object_destroy(p->value);
        free(p);
    }
}

static void
ipc_array_destroy(struct ipc_object *dict)
{
    struct ipc_object *p, *ptmp;
    struct ipc_array_head *head;
    
    head = &dict->xo_array;
    
    TAILQ_FOREACH_SAFE(p, head, xo_link, ptmp) {
        TAILQ_REMOVE(head, p, xo_link);
        ipc_object_destroy(p);
    }
}

static int
ipc_pack(struct ipc_object *xo, void **buf, uint64_t id, size_t *size)
{
    struct ipc_frame_header *header;
    mpack_writer_t writer;
    char *packed, *ret;
    size_t packed_size;
    
    mpack_writer_init_growable(&writer, &packed, &packed_size);
    xpc2mpack(&writer, xo);
    
    if (mpack_writer_destroy(&writer) != mpack_ok)
        return (-1);
    
    ret = malloc(packed_size + sizeof(*header));
    memset(ret, 0, packed_size + sizeof(*header));
    
    header = (struct ipc_frame_header *)ret;
    header->length = packed_size;
    header->id = id;
    header->version = IPC_PROTOCOL_VERSION;
    
    memcpy(ret + sizeof(*header), packed, packed_size);
    *buf = ret;
    *size = packed_size + sizeof(*header);
    
    free(packed);
    return (0);
}

static struct ipc_object *
ipc_unpack(void *buf, size_t size)
{
    mpack_tree_t tree;
    struct ipc_object *xo;
    
    mpack_tree_init(&tree, (const char *)buf, size);
    if (mpack_tree_error(&tree) != mpack_ok) {
        debugf("unpack failed: %d", mpack_tree_error(&tree))
        return (NULL);
    }
    mpack_tree_parse(&tree);
    xo = mpack2xpc(mpack_tree_root(&tree));
    mpack_tree_destroy(&tree);
    return (xo);
}

void
ipc_object_destroy(struct ipc_object *xo)
{
    if (xo->xo_ipc_type == _IPC_TYPE_DICTIONARY)
        ipc_dictionary_destroy(xo);
    
    if (xo->xo_ipc_type == _IPC_TYPE_ARRAY)
        ipc_array_destroy(xo);
    
    if (xo->xo_ipc_type == _IPC_TYPE_STRING)
        free(xo->xo_u.str);
    
    if (xo->xo_ipc_type == _IPC_TYPE_DATA)
        free((void *)xo->xo_u.ptr);
    
    free(xo);
}

ipc_object_t
ipc_retain(ipc_object_t obj)
{
    struct ipc_object *xo;
    
    xo = obj;
#ifdef __APPLE__
    OSAtomicAdd32(1,(volatile int32_t *)&xo->xo_refcnt);
#else
    atomic_add_int(&xo->xo_refcnt, 1);
#endif
    
    return (obj);
}

void
ipc_release(ipc_object_t obj)
{
    struct ipc_object *xo;
    
    xo = obj;
#ifdef __APPLE__
    if (OSAtomicAdd32(-1,(volatile int32_t *)&xo->xo_refcnt) > 0) {
        return;
    }
#else
    if (atomic_fetchadd_int(&xo->xo_refcnt, -1) > 0)
        return;
#endif
    ipc_object_destroy(xo);
}

char * ipc_copy_description(ipc_object_t obj)
{
    char *result;
    struct sbuf *sbuf;
    
    sbuf = sbuf_new_auto();
    ipc_copy_description_level(obj, sbuf, 0);
    sbuf_finish(sbuf);
    result = strdup(sbuf_data(sbuf));
    sbuf_delete(sbuf);
    
    return (result);
}

static void ipc_copy_description_level(ipc_object_t obj, struct sbuf *sbuf, int level)
{
    struct ipc_object *xo = obj;
    struct uuid *id;
    char *uuid_str;
    uint32_t uuid_status;
    
    if (obj == NULL) {
        sbuf_printf(sbuf, "<null value>\n");
        return;
    }
    
    sbuf_printf(sbuf, "(%s) ", _ipc_get_type_name(obj));
    
    switch (xo->xo_ipc_type) {
        case _IPC_TYPE_DICTIONARY:
            sbuf_printf(sbuf, "\n");
            ipc_dictionary_apply(xo, ^(const char *k, ipc_object_t v) {
                sbuf_printf(sbuf, "%*s\"%s\": ", level * 4, " ", k);
                ipc_copy_description_level(v, sbuf, level + 1);
                return ((bool)true);
            });
            break;
            
        case _IPC_TYPE_ARRAY:
            sbuf_printf(sbuf, "\n");
            ipc_array_apply(xo, ^(size_t idx, ipc_object_t v) {
                sbuf_printf(sbuf, "%*s%ld: ", level * 4, " ", idx);
                ipc_copy_description_level(v, sbuf, level + 1);
                return ((bool)true);
            });
            break;
            
        case _IPC_TYPE_BOOL:
            sbuf_printf(sbuf, "%s\n", ipc_bool_get_value(obj) ? "true" : "false");
            break;
            
        case _IPC_TYPE_STRING:
            sbuf_printf(sbuf, "\"%s\"\n", ipc_string_get_string_ptr(obj));
            break;
            
        case _IPC_TYPE_INT64:
            sbuf_printf(sbuf, "%lld\n", ipc_int64_get_value(obj));
            break;
            
        case _IPC_TYPE_UINT64:
            sbuf_printf(sbuf, "%llx\n", ipc_uint64_get_value(obj));
            break;
            
        case _IPC_TYPE_DATE:
            sbuf_printf(sbuf, "%lld\n", ipc_date_get_value(obj));
            break;
            
        case _IPC_TYPE_UUID:
            id = (struct uuid *)ipc_uuid_get_bytes(obj);
            uuid_to_string(id, &uuid_str, &uuid_status);
            sbuf_printf(sbuf, "%s\n", uuid_str);
            free(uuid_str);
            break;
            
        case _IPC_TYPE_DATA:
            sbuf_printf(sbuf, "%p\n", ipc_data_get_bytes_ptr(obj));
            break;
            
            //	case _IPC_TYPE_ENDPOINT:
            //		sbuf_printf(sbuf, "<%lld>\n", xo->xo_int);
            //		break;
            
        case _IPC_TYPE_NULL:
            sbuf_printf(sbuf, "<null>\n");
            break;
    }
}

int
ipc_pipe_send(ipc_object_t xobj, uint64_t id, ipc_port_t local)
{
    struct ipc_transport *transport = ipc_get_transport();
    void *buf;
    size_t size;
    
    assert(ipc_get_type(xobj) == &_ipc_type_dictionary);
    
    if (ipc_pack(xobj, &buf, id, &size) != 0) {
        debugf("pack failed");
        return (-1);
    }
    
    if (transport->xt_send(local, buf, size) != 0) {
        debugf("transport send function failed: %s", strerror(errno));
        free(buf);
        return (-1);
    }
    
    free(buf);
    return (0);
}

size_t
ipc_pipe_receive(ipc_port_t local, ipc_object_t *result, uint64_t *id)
{
    struct ipc_transport *transport = ipc_get_transport();
    //	struct ipc_resource *resources;
    struct ipc_frame_header *header;
    void *buffer;
    //	size_t nresources;
    size_t ret;
    
    buffer = malloc(RECV_BUFFER_SIZE);
    
    ret = transport->xt_recv(local, buffer, RECV_BUFFER_SIZE);
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
    
    header = (struct ipc_frame_header *)buffer;
    if (header->length > (ret - sizeof(*header))) {
        debugf("invalid message length");
        free(buffer);
        return (-1);
    }
    
    if (header->version != IPC_PROTOCOL_VERSION) {
        debugf("invalid protocol version")
        free(buffer);
        return (-1);
    }
    
    *id = header->id;
    
    debugf("length=%lld", header->length);
    
    *result = ipc_unpack(buffer + sizeof(*header), (size_t)header->length);
    
    if (*result == NULL){
        free(buffer);
        return (-1);
    }
    
    free(buffer);
    return (ret);
}
