//
//  ipc_dictionary.h
//  xpc_lite
//
//  Created by h4ck on 2020/11/14.
//  Copyright © 2020 猿码工作室（https://ymlab.net）. All rights reserved.
//

#ifndef ipc_dictionary_h
#define ipc_dictionary_h

#include <ipc/base.h>

__BEGIN_DECLS

typedef bool (^ipc_dictionary_applier_t)(char *key, ipc_object_t value);
bool ipc_dictionary_apply(ipc_object_t xdict, ipc_dictionary_applier_t applier);

ipc_object_t ipc_dictionary_create(char * const *keys, const ipc_object_t *values, size_t count);

ipc_object_t ipc_dictionary_create_reply(ipc_object_t original);

void ipc_dictionary_set_value(ipc_object_t xdict, char *key, ipc_object_t value);

ipc_object_t ipc_dictionary_get_value(ipc_object_t xdict, char *key);

size_t ipc_dictionary_get_count(ipc_object_t xdict);

void ipc_dictionary_set_bool(ipc_object_t xdict, char *key, bool value);

void ipc_dictionary_set_int64(ipc_object_t xdict, char *key, int64_t value);

void ipc_dictionary_set_uint64(ipc_object_t xdict, char *key, uint64_t value);

void ipc_dictionary_set_double(ipc_object_t xdict, char *key, double value);

void ipc_dictionary_set_date(ipc_object_t xdict, char *key, int64_t value);

void ipc_dictionary_set_data(ipc_object_t xdict, char *key, const void *bytes, size_t length);

void ipc_dictionary_set_string(ipc_object_t xdict, char *key,
    const char *string);

void ipc_dictionary_set_uuid(ipc_object_t xdict, char *key, const uuid_t uuid);

bool ipc_dictionary_get_bool(ipc_object_t xdict, char *key);

int64_t ipc_dictionary_get_int64(ipc_object_t xdict, char *key);

uint64_t ipc_dictionary_get_uint64(ipc_object_t xdict, char *key);

double ipc_dictionary_get_double(ipc_object_t xdict, char *key);

int64_t ipc_dictionary_get_date(ipc_object_t xdict, char *key);

const void * ipc_dictionary_get_data(ipc_object_t xdict, char *key, size_t *length);

const char * ipc_dictionary_get_string(ipc_object_t xdict, char *key);

const uint8_t * ipc_dictionary_get_uuid(ipc_object_t xdict, char *key);

__END_DECLS

#endif /* ipc_dictionary_h */
