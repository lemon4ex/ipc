//
//  ipc_array.h
//  ipc
//
//  Created by h4ck on 2020/11/14.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#ifndef ipc_array_h
#define ipc_array_h

#include <ipc/base.h>

__BEGIN_DECLS

typedef bool (^ipc_array_applier_t)(size_t index, ipc_object_t value);

ipc_object_t ipc_array_create(const ipc_object_t *objects, size_t count);

void ipc_array_set_value(ipc_object_t xarray, size_t index, ipc_object_t value);

void ipc_array_append_value(ipc_object_t xarray, ipc_object_t value);

size_t ipc_array_get_count(ipc_object_t xarray);

ipc_object_t ipc_array_get_value(ipc_object_t xarray, size_t index);

bool ipc_array_apply(ipc_object_t xarray, ipc_array_applier_t applier);

#define IPC_ARRAY_APPEND ((size_t)(-1))

void ipc_array_set_bool(ipc_object_t xarray, size_t index, bool value);

void ipc_array_set_int64(ipc_object_t xarray, size_t index, int64_t value);

void ipc_array_set_uint64(ipc_object_t xarray, size_t index, uint64_t value);

void ipc_array_set_double(ipc_object_t xarray, size_t index, double value);

void ipc_array_set_date(ipc_object_t xarray, size_t index, int64_t value);

void ipc_array_set_data(ipc_object_t xarray, size_t index, const void *bytes, size_t length);

void ipc_array_set_string(ipc_object_t xarray, size_t index, const char *string);

void ipc_array_set_uuid(ipc_object_t xarray, size_t index, const uuid_t uuid);

bool ipc_array_get_bool(ipc_object_t xarray, size_t index);

int64_t ipc_array_get_int64(ipc_object_t xarray, size_t index);

uint64_t ipc_array_get_uint64(ipc_object_t xarray, size_t index);

double ipc_array_get_double(ipc_object_t xarray, size_t index);

int64_t ipc_array_get_date(ipc_object_t xarray, size_t index);

const void * ipc_array_get_data(ipc_object_t xarray, size_t index, size_t *length);

const char * ipc_array_get_string(ipc_object_t xarray, size_t index);

const uint8_t * ipc_array_get_uuid(ipc_object_t xarray, size_t index);

__END_DECLS

#endif /* ipc_array_h */
