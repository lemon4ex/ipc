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
#include "ipc_array.h"
#include "ipc_internal.h"

ipc_object_t
ipc_array_create(const ipc_object_t *objects, size_t count)
{
    struct ipc_object *xo;
    size_t i;
    ipc_u val = {0};
    
    xo = _ipc_prim_create(_IPC_TYPE_ARRAY, val, 0);
    
    for (i = 0; i < count; i++)
        ipc_array_append_value(xo, objects[i]);
    
    return (xo);
}

void
ipc_array_set_value(ipc_object_t xarray, size_t index, ipc_object_t value)
{
    struct ipc_object *xo, *xotmp, *xotmp2;
    struct ipc_array_head *arr;
    size_t i;
    
    xo = xarray;
    arr = &xo->xo_array;
    i = 0;
    
    if (index == IPC_ARRAY_APPEND)
        return ipc_array_append_value(xarray, value);
    
    if (index >= (size_t)xo->xo_size)
        return;
    
    TAILQ_FOREACH_SAFE(xotmp, arr, xo_link, xotmp2) {
        if (i++ == index) {
            TAILQ_INSERT_AFTER(arr, (struct ipc_object *)value,
                               xotmp, xo_link);
            TAILQ_REMOVE(arr, xotmp, xo_link);
            ipc_retain(value);
            ipc_release(xotmp);
            break;
        }
    }
}

void
ipc_array_append_value(ipc_object_t xarray, ipc_object_t value)
{
    struct ipc_object *xo;
    struct ipc_array_head *arr;
    
    xo = xarray;
    arr = &xo->xo_array;
    
    TAILQ_INSERT_TAIL(arr, (struct ipc_object *)value, xo_link);
    ipc_retain(value);
}


ipc_object_t
ipc_array_get_value(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xo, *xotmp;
    struct ipc_array_head *arr;
    size_t i;
    
    xo = xarray;
    arr = &xo->xo_array;
    i = 0;
    
    if (index > xo->xo_size)
        return (NULL);
    
    TAILQ_FOREACH(xotmp, arr, xo_link) {
        if (i++ == index)
            return (xotmp);
    }
    
    return (NULL);
}

size_t
ipc_array_get_count(ipc_object_t xarray)
{
    struct ipc_object *xo;
    
    xo = xarray;
    return (xo->xo_size);
}

void
ipc_array_set_bool(ipc_object_t xarray, size_t index, bool value)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_bool_create(value);
    return (ipc_array_set_value(xarray, index, xotmp));
}


void
ipc_array_set_int64(ipc_object_t xarray, size_t index, int64_t value)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_int64_create(value);
    return (ipc_array_set_value(xarray, index, xotmp));
}

void
ipc_array_set_uint64(ipc_object_t xarray, size_t index, uint64_t value)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_uint64_create(value);
    return (ipc_array_set_value(xarray, index, xotmp));
}

void
ipc_array_set_double(ipc_object_t xarray, size_t index, double value)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_double_create(value);
    return (ipc_array_set_value(xarray, index, xotmp));
}

void
ipc_array_set_date(ipc_object_t xarray, size_t index, int64_t value)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_date_create(value);
    return (ipc_array_set_value(xarray, index, xotmp));
}

void
ipc_array_set_data(ipc_object_t xarray, size_t index, const void *data,
                   size_t length)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_data_create(data, length);
    return (ipc_array_set_value(xarray, index, xotmp));
}

void
ipc_array_set_string(ipc_object_t xarray, size_t index, const char *string)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_string_create(string);
    return (ipc_array_set_value(xarray, index, xotmp));
}

void
ipc_array_set_uuid(ipc_object_t xarray, size_t index, const uuid_t value)
{
    struct ipc_object *xo, *xotmp;
    
    xo = xarray;
    xotmp = ipc_uuid_create(value);
    return (ipc_array_set_value(xarray, index, xotmp));
}

//void
//ipc_array_set_fd(ipc_object_t xarray, size_t index, int value)
//{
//	struct ipc_object *xo, *xotmp;
//
//	xo = xarray;
//	//xotmp = ipc_fd_create(value);
//	return (ipc_array_set_value(xarray, index, xotmp));
//}
//
//void
//ipc_array_set_connection(ipc_object_t xarray, size_t index,
//    ipc_connection_t value)
//{
//
//}

bool
ipc_array_get_bool(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    return (ipc_bool_get_value(xotmp));
}

int64_t
ipc_array_get_int64(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    return (ipc_int64_get_value(xotmp));
}

uint64_t
ipc_array_get_uint64(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    return (ipc_uint64_get_value(xotmp));
}

double
ipc_array_get_double(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    return (ipc_double_get_value(xotmp));
}

int64_t
ipc_array_get_date(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    return (ipc_date_get_value(xotmp));
}

const void *
ipc_array_get_data(ipc_object_t xarray, size_t index, size_t *length)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    *length = ipc_data_get_length(xotmp);
    return (ipc_data_get_bytes_ptr(xotmp));
}

const uint8_t *
ipc_array_get_uuid(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    return (ipc_uuid_get_bytes(xotmp));
}

const char *
ipc_array_get_string(ipc_object_t xarray, size_t index)
{
    struct ipc_object *xotmp;
    
    xotmp = ipc_array_get_value(xarray, index);
    return (ipc_string_get_string_ptr(xotmp));
}

bool
ipc_array_apply(ipc_object_t xarray, ipc_array_applier_t applier)
{
    struct ipc_object *xo, *xotmp;
    struct ipc_array_head *arr;
    size_t i;
    
    i = 0;
    xo = xarray;
    arr = &xo->xo_array;
    
    TAILQ_FOREACH(xotmp, arr, xo_link) {
        if (!applier(i++, xotmp))
            return (false);
    }
    
    return (true);
}
