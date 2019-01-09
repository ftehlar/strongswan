/*
 * Copyright (c) 2019 Cisco and/or its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>
#include <stdarg.h>

#include "vaa_json.h"
#include "nxjson.h"

#define CHUNK_SIZE 5
#define GET_FIELD(_message, _offset, _type) \
    ((_type)(((uint8_t *)_message) + _offset))

#define E_OK        0
#define E_FAIL      1
#define E_NO_VAL    2

static void * vaa_nxjson_to_proto(const nx_json *json,
                    const ProtobufCMessageDescriptor *desc);

static char*
string_append(char** s, const char* format, ...)
{
    int n, size = CHUNK_SIZE;
    char *p = NULL, *np;
    va_list ap;

    p = malloc(size);
    while (1)
    {
        /* Try to print in the allocated space */
        va_start(ap, format);
        n = vsnprintf(p, size, format, ap);
        va_end(ap);

        /* Check error code */
        if (n < 0)
            return NULL;

        /* If that worked, return the string */
        if (n < size)
            break;

        /* Else try again with more space */
        size = n + CHUNK_SIZE;

        if ((np = realloc(p, size)) == NULL)
        {
            free(p);
            return NULL;
        }
        else
        {
            p = np;
        }
    }

    if (!*s)
    {
        *s = p;
    }
    else
    {
        size_t len2 = strlen(p);
        size_t len1 = strlen(*s);
        if ((np = realloc(*s, len1 + len2 + 1)) == NULL)
        {
            free(*s);
            free(p);
            return NULL;
        }
        else
        {
            *s = np;
        }
        strcat(*s, p);
        free(p);
    }
    return *s;
}

#define _sa string_append

static uint32_t
vaa_get_sizeof(ProtobufCType type)
{
    switch(type)
    {
    case PROTOBUF_C_TYPE_UINT64:
    case PROTOBUF_C_TYPE_INT64:
        return sizeof(uint64_t);
    case PROTOBUF_C_TYPE_INT32:
    case PROTOBUF_C_TYPE_UINT32:
    case PROTOBUF_C_TYPE_ENUM:
    case PROTOBUF_C_TYPE_BOOL:
        return sizeof(uint32_t);
    default:
        assert(0 && "unknown ProtobufCType while computing size of message!");
        break;
    }
    return 0;
}

static int
enum_val_exists(const ProtobufCEnumValue *t,
                unsigned size,
                int value)
{
    unsigned i;
    for (i = 0; i < size; i++)
    {
        if (t[i].value == value)
            return 1;
    }
    return 0;
}

static int
emit_value(char **out,
           void *field,
           const ProtobufCFieldDescriptor *fd,
           void *msg)
{
    const ProtobufCEnumDescriptor *enum_desc;
    const ProtobufCEnumValue *enum_val;
    int value;
    char *s;
    int has_field;

    switch (fd->type)
    {
    case PROTOBUF_C_TYPE_INT32:
        has_field = GET_FIELD(msg, fd->quantifier_offset, int *)[0];
        if (!has_field)
            return E_NO_VAL;
        _sa(out, "%d", *(int32_t *)field);
        break;
    case PROTOBUF_C_TYPE_UINT32:
        has_field = GET_FIELD(msg, fd->quantifier_offset, int *)[0];
        if (!has_field)
            return E_NO_VAL;
        _sa(out, "%u", *(int32_t *)field);
        break;
    case PROTOBUF_C_TYPE_INT64:
        has_field = GET_FIELD(msg, fd->quantifier_offset, int *)[0];
        if (!has_field)
            return E_NO_VAL;
        _sa(out, "%lld", *(int64_t *)field);
        break;
    case PROTOBUF_C_TYPE_UINT64:
        has_field = GET_FIELD(msg, fd->quantifier_offset, int *)[0];
        if (!has_field)
            return E_NO_VAL;
        _sa(out, "%llu", *(uint64_t *)field);
        break;
    case PROTOBUF_C_TYPE_STRING:
        if (*(char **)field)
        {
            _sa(out, "\"%s\"", *(char **)field);
        }
        else
        {
            return E_NO_VAL;
        }
        break;
    case PROTOBUF_C_TYPE_MESSAGE:
        assert(fd->descriptor);
        if (*(void **)field)
        {
            s = vaa_to_json(*(void **)field);
            if (!s)
            {
                return E_FAIL;
            }
            _sa(out, "%s", s);
            free(s);
        }
        else
        {
            return E_NO_VAL;
        }
        break;
    case PROTOBUF_C_TYPE_ENUM:
        has_field = GET_FIELD(msg, fd->quantifier_offset, int *)[0];
        if (!has_field)
            return E_NO_VAL;
        assert(fd->descriptor);
        enum_desc = fd->descriptor;
        enum_val = enum_desc->values;
        value = *(int *)field;
        if (!enum_val_exists(enum_val, enum_desc->n_values, value))
        {
            return E_NO_VAL;
        }
        _sa(out, "%d", value);
        break;
    case PROTOBUF_C_TYPE_BOOL:
        has_field = GET_FIELD(msg, fd->quantifier_offset, int *)[0];
        if (!has_field)
            return E_NO_VAL;
        if (*(int32_t *)field) {
            _sa(out, "%s", "true");
        } else {
            _sa(out, "%s", "false");
        }
        break;
    default:
        return E_FAIL;
    }
    return E_OK;
}

char *
vaa_to_json(ProtobufCMessage *msg)
{
    const ProtobufCMessageDescriptor *desc = msg->descriptor;
    char *out_ = NULL, **out = &out_;
    size_t n, j;
    uint32_t i;
    const ProtobufCFieldDescriptor *f;
    void *field;
    int rc;
    int first = 1;
    char *value;

    if (NULL == msg)
    {
        return NULL;
    }

    _sa(out, "{");

    for (i = 0; i < desc->n_fields; i++)
    {
        f = &desc->fields[i];

        if (f->flags & PROTOBUF_C_FIELD_FLAG_ONEOF)
        {
            n = GET_FIELD(msg, f->quantifier_offset, size_t *)[0];
            if (n != f->id)
            {
                /* not the case we want */
                continue;
            }
        }
        value = NULL;
        rc = E_NO_VAL;

        if (f->label == PROTOBUF_C_LABEL_REPEATED)
        {
            n = GET_FIELD(msg, f->quantifier_offset, size_t *)[0];

            if (n > 0) {
                _sa(&value, "[");
                for (j = 0; j < n; j++)
                {

                    if (PROTOBUF_C_TYPE_STRING == f->type ||
                            PROTOBUF_C_TYPE_MESSAGE == f->type)
                    {
                        field = GET_FIELD(msg, f->offset, void ***)[0] + j;
                    }
                    else
                    {
                        uint32_t sizeof_item = vaa_get_sizeof(f->type);
                        if (0 == sizeof_item)
                        {
                            if (*out) free(*out);
                            if (value) free(value);
                            return NULL;
                        }
                        field = GET_FIELD(msg, f->offset, void**)[0];
                        field = ((uint8_t *)field) + j * sizeof_item;
                    }
                    rc = emit_value(&value, field, f, NULL);
                    if (rc == E_FAIL)
                    {
                        if (*out) free(*out);
                        if (value) free(value);
                        return NULL;
                    }

                    if (j != n - 1)
                        _sa(&value, ",");
                }
                _sa(&value, "]");
            }
        }
        else
        {
            rc = emit_value(&value, GET_FIELD(msg, f->offset, void *), f, msg);
            if (rc == E_FAIL)
            {
                if (*out) free(*out);
                if (value) free(value);
                return NULL;
            }
        }

        if (rc == E_OK) {
            if (!first)
                _sa(out, ",");

            _sa(out, "\"%s\":%s", f->name, value);
            first = 0;
        }
        if (value) free(value);
    }

    _sa(out, "}");
    return *out;
}

static int
vaa_set_field(void *field,
              const nx_json *json,
              const ProtobufCFieldDescriptor *f,
              void *message)
{
    void *ptr;

    switch (f->type) {
    case PROTOBUF_C_TYPE_INT32:
        ((int32_t *)field)[0] = json->int_value;
        GET_FIELD(message, f->quantifier_offset, int *)[0] = 1;
        break;
    case PROTOBUF_C_TYPE_UINT32:
        ((uint32_t *)field)[0] = json->int_value;
        GET_FIELD(message, f->quantifier_offset, int *)[0] = 1;
        break;
    case PROTOBUF_C_TYPE_UINT64:
        ((uint64_t *)field)[0] = json->int_value;
        GET_FIELD(message, f->quantifier_offset, int *)[0] = 1;
        break;
    case PROTOBUF_C_TYPE_INT64:
        ((uint64_t *)field)[0] = json->int_value;
        GET_FIELD(message, f->quantifier_offset, int *)[0] = 1;
        break;
    case PROTOBUF_C_TYPE_STRING:
        if (json->text_value)
        {
            ptr = strdup((const char *)json->text_value);
            memcpy(((char **)field), &ptr, sizeof(char *));
        }
        break;
    case PROTOBUF_C_TYPE_MESSAGE:
        assert(f->descriptor);
        ptr = vaa_nxjson_to_proto(json, f->descriptor);
        if (!ptr)
            return E_FAIL;

        memcpy((void **)field, &ptr, sizeof(void *));
        break;
    case PROTOBUF_C_TYPE_ENUM:
        ((int32_t *)field)[0] = json->int_value;
        GET_FIELD(message, f->quantifier_offset, int *)[0] = 1;
        break;
    case PROTOBUF_C_TYPE_BOOL:
        ((int *)field)[0] = json->int_value;
        GET_FIELD(message, f->quantifier_offset, int *)[0] = 1;
        break;
    default:
        return E_FAIL;
    }
    return E_OK;
}

static void *
vaa_nxjson_to_proto(const nx_json *json,
                    const ProtobufCMessageDescriptor *desc)
{
    unsigned i, j, n;
    const ProtobufCFieldDescriptor *f;
    void **repeated_pp = NULL, *repeated_p = NULL;

    uint8_t *message = malloc(desc->sizeof_message);
    desc->message_init((ProtobufCMessage *)message);

    for (i = 0; i < desc->n_fields; i++)
    {
        f = &desc->fields[i];

        const nx_json* json_field = nx_json_get(json, f->name);
        if (json_field->type == NX_JSON_NULL)
        {
            continue;
        }

        if (f->label == PROTOBUF_C_LABEL_REPEATED)
        {
            n = json_field->length;
            if (0 == n)
                break;

            GET_FIELD(message, f->quantifier_offset, size_t *)[0] = n;
            assert(0 == (f->flags & PROTOBUF_C_FIELD_FLAG_ONEOF));

            if (PROTOBUF_C_TYPE_STRING == f->type ||
                    PROTOBUF_C_TYPE_MESSAGE == f->type)
            {
                repeated_pp = malloc(n * sizeof(void *));
                memcpy(GET_FIELD(message, f->offset, void ***), &repeated_pp,
                        sizeof(void **));
            }
            else
            {
                uint32_t sizeof_item = vaa_get_sizeof(f->type);
                if (sizeof_item == 0)
                {
                    fprintf(stderr, "unknown size of element %s!", f->name);
                    return NULL;
                }
                repeated_p = malloc(n * sizeof_item);
                memcpy(GET_FIELD(message, f->offset, void **), &repeated_p,
                        sizeof(void *));
            }

            for (j = 0; j < n; j++)
            {
                const nx_json *json_item = nx_json_item(json_field, j);
                if (PROTOBUF_C_TYPE_STRING == f->type ||
                        PROTOBUF_C_TYPE_MESSAGE == f->type)
                {
                    if (E_FAIL == vaa_set_field(repeated_pp + j, json_item, f,
                                NULL))
                    {
                        return NULL;
                    }
                }
                else
                {
                    uint32_t sizeof_item = vaa_get_sizeof(f->type);
                    if (E_FAIL ==  vaa_set_field(repeated_p + j * sizeof_item,
                                    json_item, f, NULL))
                    {
                        return NULL;
                    }
                }
            }
        }
        else
        {
            if (E_FAIL == vaa_set_field(GET_FIELD(message, f->offset, void *),
                        json_field, f, message))
            {
                return NULL;
            }
        }
        if (f->flags & PROTOBUF_C_FIELD_FLAG_ONEOF)
        {
            GET_FIELD(message, f->quantifier_offset, unsigned *)[0] = f->id;
        }
    }
    return message;
}

void *
vaa_to_proto(char *msg,
             const ProtobufCMessageDescriptor *desc)
{
    const nx_json *json = nx_json_parse(msg, 0);
    if (!json)
        return NULL;

    char *result = vaa_nxjson_to_proto(json, desc);
    nx_json_free(json);
    return result;
}

