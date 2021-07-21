//
//  ipc_type.h
//  ipc
//
//  Created by h4ck on 2020/11/14.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#include <sys/types.h>
#include <time.h>
#include <sys/time.h>
#include "base.h"
#include "ipc_internal.h"
#include "ipc_array.h"
#include "ipc_dictionary.h"

struct _ipc_type_s
{
};

typedef const struct _ipc_type_s xt;
xt _ipc_type_array;
xt _ipc_type_bool;
xt _ipc_type_data;
xt _ipc_type_date;
xt _ipc_type_dictionary;
xt _ipc_type_null;
xt _ipc_type_error;
xt _ipc_type_int64;
xt _ipc_type_uint64;
xt _ipc_type_string;
xt _ipc_type_uuid;
xt _ipc_type_double;

struct _ipc_bool_s
{
};

typedef const struct _ipc_bool_s xb;

xb _ipc_bool_true;
xb _ipc_bool_false;

struct _ipc_dictionary_s {
    
};

typedef const struct _ipc_dictionary_s xs;
xs _ipc_error_connection_invalid;

static size_t ipc_data_hash(const uint8_t *data, size_t length);

static ipc_type_t ipc_typemap[] = {
    NULL,
    IPC_TYPE_DICTIONARY,
    IPC_TYPE_ARRAY,
    IPC_TYPE_BOOL,
    IPC_TYPE_NULL,
    NULL,
    IPC_TYPE_INT64,
    IPC_TYPE_UINT64,
    IPC_TYPE_DATE,
    IPC_TYPE_DATA,
    IPC_TYPE_STRING,
    IPC_TYPE_UUID,
    IPC_TYPE_ERROR,
    IPC_TYPE_DOUBLE};

static const char *ipc_typestr[] = {
    "invalid",
    "dictionary",
    "array",
    "bool",
    "null",
    "invalid",
    "int64",
    "uint64",
    "date",
    "data",
    "string",
    "uuid",
    "error",
    "double"};

__private_extern__ struct ipc_object *
_ipc_prim_create(int type, ipc_u value, size_t size)
{
    return (_ipc_prim_create_flags(type, value, size, 0));
}

__private_extern__ struct ipc_object *
_ipc_prim_create_flags(int type, ipc_u value, size_t size, uint16_t flags)
{
    struct ipc_object *xo;

    if ((xo = malloc(sizeof(*xo))) == NULL)
        return (NULL);

    xo->xo_size = size;
    xo->xo_ipc_type = type;
    xo->xo_flags = flags;
    xo->xo_u = value;
    xo->xo_refcnt = 1;

    if (type == _IPC_TYPE_DICTIONARY)
        TAILQ_INIT(&xo->xo_dict);

    if (type == _IPC_TYPE_ARRAY)
        TAILQ_INIT(&xo->xo_array);

    return (xo);
}

ipc_object_t ipc_null_create(void)
{
    ipc_u val = {0};
    return _ipc_prim_create(_IPC_TYPE_NULL, val, 0);
}

ipc_object_t ipc_bool_create(bool value)
{
    ipc_u val = {0};

    val.b = value;
    return _ipc_prim_create(_IPC_TYPE_BOOL, val, 1);
}

bool ipc_bool_get_value(ipc_object_t xbool)
{
    struct ipc_object *xo;

    xo = xbool;
    if (xo == NULL)
        return (0);

    if (xo->xo_ipc_type == _IPC_TYPE_BOOL)
        return (xo->xo_bool);

    return (false);
}

ipc_object_t ipc_int64_create(int64_t value)
{
    ipc_u val = {0};

    val.i = value;
    return _ipc_prim_create(_IPC_TYPE_INT64, val, 1);
}

int64_t ipc_int64_get_value(ipc_object_t xint)
{
    struct ipc_object *xo;

    xo = xint;
    if (xo == NULL)
        return (0);

    if (xo->xo_ipc_type == _IPC_TYPE_INT64)
        return (xo->xo_int);

    return (0);
}

ipc_object_t ipc_uint64_create(uint64_t value)
{
    ipc_u val = {0};

    val.ui = value;
    return _ipc_prim_create(_IPC_TYPE_UINT64, val, 1);
}

uint64_t ipc_uint64_get_value(ipc_object_t xuint)
{
    struct ipc_object *xo;

    xo = xuint;
    if (xo == NULL)
        return (0);

    if (xo->xo_ipc_type == _IPC_TYPE_UINT64)
        return (xo->xo_uint);

    return (0);
}

ipc_object_t ipc_double_create(double value)
{
    ipc_u val = {0};

    val.d = value;
    return _ipc_prim_create(_IPC_TYPE_DOUBLE, val, 1);
}

double ipc_double_get_value(ipc_object_t xdouble)
{
    struct ipc_object *xo = xdouble;

    if (xo->xo_ipc_type == _IPC_TYPE_DOUBLE)
        return (xo->xo_d);

    return (0);
}

ipc_object_t ipc_date_create(int64_t interval)
{
    ipc_u val = {0};

    val.i = interval;
    return _ipc_prim_create(_IPC_TYPE_DATE, val, 1);
}

ipc_object_t ipc_date_create_from_current(void)
{
    ipc_u val = {0};

    if (__builtin_available(iOS 10.0, *))
    {
        struct timespec tp;
        clock_gettime(CLOCK_REALTIME, &tp);
        val.ui = *(uint64_t *)&tp;
    }
    else
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        val.ui = (uint64_t)tv.tv_sec * 1000 + (uint64_t)tv.tv_usec / 1000;
    }

    return _ipc_prim_create(_IPC_TYPE_DATE, val, 1);
}

int64_t ipc_date_get_value(ipc_object_t xdate)
{
    struct ipc_object *xo = xdate;

    if (xo == NULL)
        return (0);

    if (xo->xo_ipc_type == _IPC_TYPE_DATE)
        return (xo->xo_int);

    return (0);
}

ipc_object_t ipc_data_create(const void *bytes, size_t length)
{
    ipc_u val = {0};

    void *value = malloc(length);
    memcpy(value, bytes, length);
    val.ptr = (uintptr_t)value;
    return _ipc_prim_create(_IPC_TYPE_DATA, val, length);
}

size_t ipc_data_get_length(ipc_object_t xdata)
{
    struct ipc_object *xo = xdata;

    if (xo == NULL)
        return (0);

    if (xo->xo_ipc_type == _IPC_TYPE_DATA)
        return (xo->xo_size);

    return (0);
}

const void *ipc_data_get_bytes_ptr(ipc_object_t xdata)
{
    struct ipc_object *xo = xdata;

    if (xo == NULL)
        return (NULL);

    if (xo->xo_ipc_type == _IPC_TYPE_DATA)
        return ((const void *)xo->xo_ptr);

    return (0);
}

size_t ipc_data_get_bytes(ipc_object_t xdata, void *buffer, size_t off, size_t length)
{
    struct ipc_object *xo = xdata;

    if (xo == NULL)
    {
        return 0;
    }
    if (xo->xo_ipc_type != _IPC_TYPE_DATA)
    {
        return 0;
    }
    if (off > xo->xo_size)
    {
        return 0;
    }
    size_t len = MIN(length, xo->xo_size - off);
    memcpy(buffer, ((char *)xo->xo_u.ptr) + off, len);
    return len;
}

ipc_object_t ipc_string_create(const char *string)
{
    ipc_u val = {0};

    val.str = __DECONST(char *, strdup(string));
    return _ipc_prim_create(_IPC_TYPE_STRING, val, strlen(string));
}

ipc_object_t ipc_error_create(const void *event)
{
    ipc_u val = {0};

    val.ptr = (uintptr_t)event;
    return _ipc_prim_create(_IPC_TYPE_ERROR, val, 1);
}

ipc_object_t ipc_string_create_with_format(const char *fmt, ...)
{
    va_list ap;
    ipc_u val = {0};

    va_start(ap, fmt);
    vasprintf(&val.str, fmt, ap);
    va_end(ap);
    return _ipc_prim_create(_IPC_TYPE_STRING, val, strlen(val.str));
}

ipc_object_t ipc_string_create_with_format_and_arguments(const char *fmt, va_list ap)
{
    ipc_u val = {0};

    vasprintf(&val.str, fmt, ap);
    return _ipc_prim_create(_IPC_TYPE_STRING, val, strlen(val.str));
}

size_t ipc_string_get_length(ipc_object_t xstring)
{
    struct ipc_object *xo = xstring;

    if (xo == NULL)
        return (0);

    if (xo->xo_ipc_type == _IPC_TYPE_STRING)
        return (xo->xo_size);

    return (0);
}

const char *ipc_string_get_string_ptr(ipc_object_t xstring)
{
    struct ipc_object *xo = xstring;

    if (xo == NULL)
        return (NULL);

    if (xo->xo_ipc_type == _IPC_TYPE_STRING)
        return (xo->xo_str);

    return (NULL);
}

ipc_object_t ipc_uuid_create(const uuid_t uuid)
{
    ipc_u val = {0};

    memcpy(val.uuid, uuid, sizeof(uuid_t));
    return _ipc_prim_create(_IPC_TYPE_UUID, val, 1);
}

const uint8_t *ipc_uuid_get_bytes(ipc_object_t xuuid)
{
    struct ipc_object *xo;

    xo = xuuid;
    if (xo == NULL)
        return (NULL);

    if (xo->xo_ipc_type == _IPC_TYPE_UUID)
        return ((uint8_t *)&xo->xo_uuid);

    return (NULL);
}

ipc_type_t ipc_get_type(ipc_object_t obj)
{
    struct ipc_object *xo;

    xo = obj;
    return (ipc_typemap[xo->xo_ipc_type]);
}

bool ipc_equal(ipc_object_t x1, ipc_object_t x2)
{
    struct ipc_object *xo1, *xo2;

    xo1 = x1;
    xo2 = x2;

    if (xo1 == xo2)
    {
        return true;
    }

    if (xo1->xo_ipc_type != xo1->xo_ipc_type)
    {
        return false;
    }

    return ipc_hash(xo1) == ipc_hash(xo2);
}

ipc_object_t ipc_copy(ipc_object_t obj)
{
    struct ipc_object *xo, *xotmp;
    const void *newdata;

    xo = obj;
    switch (xo->xo_ipc_type)
    {
    case _IPC_TYPE_BOOL:
    case _IPC_TYPE_INT64:
    case _IPC_TYPE_UINT64:
    case _IPC_TYPE_DATE:
        //		case _IPC_TYPE_ENDPOINT:
        return _ipc_prim_create(xo->xo_ipc_type, xo->xo_u, 1);

    case _IPC_TYPE_STRING:
        return ipc_string_create(ipc_string_get_string_ptr(xo));

    case _IPC_TYPE_DATA:
        newdata = ipc_data_get_bytes_ptr(obj);
        return (ipc_data_create(newdata,
                                ipc_data_get_length(obj)));

    case _IPC_TYPE_DICTIONARY:
        xotmp = ipc_dictionary_create(NULL, NULL, 0);
        ipc_dictionary_apply(obj, ^(char *k, ipc_object_t v) {
          ipc_dictionary_set_value(xotmp, (char *)k, ipc_copy(v));
          return (bool)true;
        });
        return (xotmp);

    case _IPC_TYPE_ARRAY:
        xotmp = ipc_array_create(NULL, 0);
        ipc_array_apply(obj, ^(size_t idx, ipc_object_t v) {
          ipc_array_set_value(xotmp, idx, ipc_copy(v));
          return ((bool)true);
        });
        return (xotmp);
    }

    return (0);
}

static size_t ipc_data_hash(const uint8_t *data, size_t length)
{
    size_t hash = 5381;

    while (length--)
        hash = ((hash << 5) + hash) + data[length];

    return (hash);
}

size_t ipc_hash(ipc_object_t obj)
{
    struct ipc_object *xo;
    __block size_t hash = 0;

    xo = obj;
    switch (xo->xo_ipc_type)
    {
    case _IPC_TYPE_BOOL:
    case _IPC_TYPE_INT64:
    case _IPC_TYPE_UINT64:
    case _IPC_TYPE_DATE:
        //	case _IPC_TYPE_ENDPOINT:
        return ((size_t)xo->xo_u.ui);

    case _IPC_TYPE_STRING:
        return (ipc_data_hash(
            (const uint8_t *)ipc_string_get_string_ptr(obj),
            ipc_string_get_length(obj)));

    case _IPC_TYPE_DATA:
        return (ipc_data_hash(
            ipc_data_get_bytes_ptr(obj),
            ipc_data_get_length(obj)));

    case _IPC_TYPE_DICTIONARY:
        ipc_dictionary_apply(obj, ^(char *k, ipc_object_t v) {
          hash ^= ipc_data_hash((const uint8_t *)k, strlen(k));
          hash ^= ipc_hash(v);
          return ((bool)true);
        });
        return (hash);

    case _IPC_TYPE_ARRAY:
        ipc_array_apply(obj, ^(size_t idx, ipc_object_t v) {
          hash ^= ipc_hash(v);
          return ((bool)true);
        });
        return (hash);
    }

    return (0);
}

__private_extern__ const char *
_ipc_get_type_name(ipc_object_t obj)
{
    struct ipc_object *xo;

    xo = obj;
    return (ipc_typestr[xo->xo_ipc_type]);
}
