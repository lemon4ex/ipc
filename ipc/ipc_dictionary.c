//
//  ipc_dictionary.h
//  ipc
//  Created by h4ck on 2020/11/14.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#include "base.h"
#include "ipc_internal.h"
#include "ipc_dictionary.h"
#include "ipc_array.h"
#include "mpack.h"

struct ipc_object *mpack2xpc(const mpack_node_t node)
{
    ipc_object_t xotmp;
    size_t i;
    ipc_u val;

    switch (mpack_node_type(node))
    {
    case mpack_type_nil:
        xotmp = ipc_null_create();
        break;

    case mpack_type_int:
        val.i = mpack_node_i64(node);
        xotmp = ipc_int64_create(val.i);
        break;

    case mpack_type_uint:
        val.ui = mpack_node_u64(node);
        xotmp = ipc_uint64_create(val.ui);
        break;

    case mpack_type_bool:
        val.b = mpack_node_bool(node);
        xotmp = ipc_bool_create(val.b);
        break;

    case mpack_type_double:
        val.d = mpack_node_double(node);
        xotmp = ipc_double_create(val.d);
        break;

    case mpack_type_str:
        val.str = mpack_node_cstr_alloc(node, mpack_node_data_len(node) + 1);
        xotmp = ipc_string_create(val.str);
        free(val.str);
        break;

    case mpack_type_bin:
    {
        size_t len = mpack_node_bin_size(node);
        val.str = mpack_node_data_alloc(node, len);
        xotmp = ipc_data_create(val.str, len);
        free(val.str);
    }
    break;

    case mpack_type_array:
        xotmp = ipc_array_create(NULL, 0);
        for (i = 0; i < mpack_node_array_length(node); i++)
        {
            ipc_object_t item = mpack2xpc(
                mpack_node_array_at(node, i));
            ipc_array_append_value(item, xotmp);
        }
        break;

    case mpack_type_map:
    {
        xotmp = ipc_dictionary_create(NULL, NULL, 0);
        for (i = 0; i < mpack_node_map_count(node); i++)
        {
            mpack_node_t key_node = mpack_node_map_key_at(node, i);
            size_t len = mpack_node_data_len(key_node);
            char *key = mpack_node_cstr_alloc(key_node, len + 1);
            ipc_object_t value = mpack2xpc(
                mpack_node_map_value_at(node, i));
            ipc_dictionary_set_value(xotmp, key, value);
            free(key);
        }
    }
    break;
    default:
        xotmp = NULL;
        break;
    }

    return (xotmp);
}

void xpc2mpack(mpack_writer_t *writer, ipc_object_t obj)
{
    struct ipc_object *xotmp = obj;

    switch (xotmp->xo_ipc_type)
    {
    case _IPC_TYPE_DICTIONARY:
        mpack_start_map(writer, (uint32_t)ipc_dictionary_get_count(obj));
        ipc_dictionary_apply(obj, ^(char *k, ipc_object_t v) {
          mpack_write_cstr(writer, k);
          xpc2mpack(writer, v);
          return ((bool)true);
        });
        mpack_finish_map(writer);
        break;

    case _IPC_TYPE_ARRAY:
        mpack_start_array(writer, (uint32_t)ipc_array_get_count(obj));
        ipc_array_apply(obj, ^(size_t index __unused, ipc_object_t v) {
          xpc2mpack(writer, v);
          return ((bool)true);
        });
        mpack_finish_map(writer);
        break;

    case _IPC_TYPE_NULL:
        mpack_write_nil(writer);
        break;

    case _IPC_TYPE_BOOL:
        mpack_write_bool(writer, ipc_bool_get_value(obj));
        break;

    case _IPC_TYPE_INT64:
        mpack_write_i64(writer, ipc_int64_get_value(obj));
        break;

    case _IPC_TYPE_DOUBLE:
        mpack_write_double(writer, ipc_double_get_value(obj));
        break;
    case _IPC_TYPE_UINT64:
        mpack_write_u64(writer, ipc_uint64_get_value(obj));
        break;

    case _IPC_TYPE_STRING:
        mpack_write_cstr(writer, ipc_string_get_string_ptr(obj));
        break;

    case _IPC_TYPE_DATA:
        mpack_write_bin(writer, ipc_data_get_bytes_ptr(obj), (uint32_t)ipc_data_get_length(obj));
        break;
    }
}

ipc_object_t ipc_dictionary_create(char *const *keys, const ipc_object_t *values, size_t count)
{
    struct ipc_object *xo;
    size_t i;
    ipc_u val = {0};

    xo = _ipc_prim_create(_IPC_TYPE_DICTIONARY, val, count);

    for (i = 0; i < count; i++)
    {
        ipc_dictionary_set_value(xo, keys[i], values[i]);
    }
    return (xo);
}

ipc_object_t ipc_dictionary_create_reply(ipc_object_t original)
{
    struct ipc_object *xo_orig;

    xo_orig = original;
    if ((xo_orig->xo_flags & _IPC_FROM_WIRE) == 0)
        return (NULL);

    return ipc_dictionary_create(NULL, NULL, 0);
}

void ipc_dictionary_set_value(ipc_object_t xdict, char *key, ipc_object_t value)
{
    struct ipc_object *xo;
    struct ipc_dict_head *head;
    struct ipc_dict_pair *pair;

    xo = xdict;
    head = &xo->xo_dict;

    TAILQ_FOREACH(pair, head, xo_link)
    {
        if (!strcmp(pair->key, key))
        {
            pair->value = value;
            return;
        }
    }

    xo->xo_size++;
    pair = malloc(sizeof(struct ipc_dict_pair));
    pair->key = strdup(key);
    pair->value = value;
    TAILQ_INSERT_TAIL(&xo->xo_dict, pair, xo_link);
}

ipc_object_t
ipc_dictionary_get_value(ipc_object_t xdict, char *key)
{
    struct ipc_object *xo;
    struct ipc_dict_head *head;
    struct ipc_dict_pair *pair;

    xo = xdict;
    head = &xo->xo_dict;

    TAILQ_FOREACH(pair, head, xo_link)
    {
        if (!strcmp(pair->key, key))
            return (pair->value);
    }

    return (NULL);
}

size_t ipc_dictionary_get_count(ipc_object_t xdict)
{
    struct ipc_object *xo;

    xo = xdict;
    return (xo->xo_size);
}

void ipc_dictionary_set_bool(ipc_object_t xdict, char *key, bool value)
{
    ;
    struct ipc_object *xo, *xotmp;

    xo = xdict;
    xotmp = ipc_bool_create(value);
    // 为了统一释放key，这里使用strdup(key)创建一个新的key
    ipc_dictionary_set_value(xdict, key, xotmp);
}

void ipc_dictionary_set_int64(ipc_object_t xdict, char *key, int64_t value)
{
    struct ipc_object *xo, *xotmp;

    xo = xdict;
    xotmp = ipc_int64_create(value);
    ipc_dictionary_set_value(xdict, key, xotmp);
}

void ipc_dictionary_set_uint64(ipc_object_t xdict, char *key, uint64_t value)
{
    struct ipc_object *xo, *xotmp;

    xo = xdict;
    xotmp = ipc_uint64_create(value);
    ipc_dictionary_set_value(xdict, key, xotmp);
}

void ipc_dictionary_set_double(ipc_object_t xdict, char *key, double value)
{
    struct ipc_object *xo, *xotmp;

    xo = xdict;
    xotmp = ipc_double_create(value);
    ipc_dictionary_set_value(xdict, key, xotmp);
}

void ipc_dictionary_set_data(ipc_object_t xdict, char *key, const void *bytes, size_t length)
{
    struct ipc_object *xo, *xotmp;

    xo = xdict;
    xotmp = ipc_data_create(bytes, length);
    ipc_dictionary_set_value(xdict, key, xotmp);
}

void ipc_dictionary_set_date(ipc_object_t xdict, char *key, int64_t value)
{
    struct ipc_object *xo, *xotmp;

    xo = xdict;
    xotmp = ipc_date_create(value);
    ipc_dictionary_set_value(xdict, key, xotmp);
}

void ipc_dictionary_set_string(ipc_object_t xdict, char *key, const char *value)
{
    struct ipc_object *xo, *xotmp;

    xo = xdict;
    xotmp = ipc_string_create(value);
    ipc_dictionary_set_value(xdict, key, xotmp);
}

bool ipc_dictionary_get_bool(ipc_object_t xdict, char *key)
{
    ipc_object_t value = ipc_dictionary_get_value(xdict, key);
    if (value == NULL)
        return 0;

    struct ipc_object *xo = value;
    if (xo->xo_ipc_type != _IPC_TYPE_BOOL)
        return 0;

    return (ipc_bool_get_value(xo));
}

int64_t ipc_dictionary_get_int64(ipc_object_t xdict, char *key)
{
    ipc_object_t value = ipc_dictionary_get_value(xdict, key);
    if (value == NULL)
        return 0;

    struct ipc_object *xo = value;
    if (xo->xo_ipc_type != _IPC_TYPE_INT64)
        return 0;

    return (ipc_int64_get_value(xo));
}

uint64_t ipc_dictionary_get_uint64(ipc_object_t xdict, char *key)
{
    ipc_object_t value = ipc_dictionary_get_value(xdict, key);
    if (value == NULL)
        return 0;

    struct ipc_object *xo = value;
    if (xo->xo_ipc_type != _IPC_TYPE_UINT64)
        return 0;

    return (ipc_uint64_get_value(xo));
}

const char *ipc_dictionary_get_string(ipc_object_t xdict, char *key)
{
    ipc_object_t value = ipc_dictionary_get_value(xdict, key);
    if (value == NULL)
        return 0;

    struct ipc_object *xo = value;
    if (xo->xo_ipc_type != _IPC_TYPE_STRING)
        return NULL;

    return (ipc_string_get_string_ptr(value));
}

double ipc_dictionary_get_double(ipc_object_t xdict, char *key)
{
    ipc_object_t value = ipc_dictionary_get_value(xdict, key);
    if (value == NULL)
        return 0;

    struct ipc_object *xo = value;
    if (xo->xo_ipc_type != _IPC_TYPE_DOUBLE)
        return 0;

    return (ipc_double_get_value(xo));
}

bool ipc_dictionary_apply(ipc_object_t xdict, ipc_dictionary_applier_t applier)
{
    struct ipc_object *xo;
    struct ipc_dict_head *head;
    struct ipc_dict_pair *pair;

    xo = xdict;
    head = &xo->xo_dict;

    TAILQ_FOREACH(pair, head, xo_link)
    {
        if (!applier(pair->key, pair->value))
            return (false);
    }

    return (true);
}

int64_t ipc_dictionary_get_date(ipc_object_t xdict, char *key)
{
    ipc_object_t xdata = ipc_dictionary_get_value(xdict, key);
    if (xdata == NULL)
        return 0;

    struct ipc_object *xo = xdata;
    if (xo->xo_ipc_type != _IPC_TYPE_DATE)
        return 0;

    return ipc_date_get_value(xo);
}

const void *ipc_dictionary_get_data(ipc_object_t xdict, char *key, size_t *length)
{
    ipc_object_t xdata = ipc_dictionary_get_value(xdict, key);
    if (xdata == NULL)
        return NULL;

    struct ipc_object *xo = xdata;
    if (xo->xo_ipc_type != _IPC_TYPE_DATA)
        return NULL;

    if (length != NULL)
        *length = ipc_data_get_length(xdata);

    return ipc_data_get_bytes_ptr(xdata);
}

const uint8_t *ipc_dictionary_get_uuid(ipc_object_t xdict, char *key)
{
    ipc_object_t xdata = ipc_dictionary_get_value(xdict, key);
    if (xdata == NULL)
        return NULL;

    struct ipc_object *xo = xdata;
    if (xo->xo_ipc_type != _IPC_TYPE_UUID)
        return NULL;

    return ipc_uuid_get_bytes(xo);
}
