/*
 * Copyright (c) 2018-2019 Cisco and/or its affiliates.
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
#include <library.h>
#include <utils/debug.h>
#include <threading/thread.h>
#include <threading/condvar.h>
#include <threading/mutex.h>
#include <collections/array.h>
#include <collections/hashtable.h>

#include "configurator/configurator.grpc-c.h"
#include "kernel_vpp_grpc.h"
#include "hiredis/hiredis.h"
#include "vaa_json.h"

#define VPP_AGENT_DEFAULT_GRPC_HOST "localhost:9111"
#define VPP_AGENT_DEFAULT_REDIS_HOST "localhost:6380"
#define VPP_AGENT_DEFAULT_CHANNEL "grpc"
#define VPP_AGENT_DEFAULT_VPP_LABEL "vpp1"

#define REDIS_CFG "/vnf-agent/%s/config/vpp/v2/"
#define REDIS_STAT "/vnf-agent/%s/status/vpp/v2/"
#define REDIS_KEY_VPP_INTERFACE REDIS_CFG"interfaces/%s"
#define REDIS_KEY_TOHOST REDIS_CFG"tohost/l3/%d/l4/%d/port/%d"
#define REDIS_KEY_ROUTE REDIS_CFG"route/vrf/0/dst/%s/gw/%s"

#define REDIS_DUMP_PREFIX "scan %s match "REDIS_CFG
#define REDIS_DUMP_PREFIX_STAT "scan %s match "REDIS_STAT
#define REDIS_VPP_INTERFACE_DUMP REDIS_DUMP_PREFIX"interfaces/*"
#define REDIS_ROUTE_DUMP REDIS_DUMP_PREFIX"route/*"
#define REDIS_PUNTS_DUMP REDIS_DUMP_PREFIX_STAT"tohost/l3/ALL/*"

#define rpc_call(_rq, _rp, _op)                                               \
    configurator__configurator__ ## _op(                                      \
                this->grpc_client,                                            \
                NULL, /* metadata array */                                    \
                0, /* flags */                                                \
                _rq,                                                          \
                _rp,                                                          \
                NULL /* status, ignored due to vpp-agent not filling it */,   \
                -1 /* timeout */) == GRPC_C_OK ? SUCCESS : FAILED

#define redis_update(_proto_msg, _is_add, args...)                            \
do {                                                                          \
    char *json;                                                               \
    char key[128];                                                            \
    snprintf(key, sizeof(key), ##args);                                       \
    if (!_is_add)                                                             \
    {                                                                         \
       void *reply = redisCommand(this->redis_context, "del %s", key);        \
       if (NULL == reply)                                                     \
       {                                                                      \
           DBG1(DBG_KNL, "error on del command: %s",                          \
                   this->redis_context->errstr);                              \
           return FAILED;                                                     \
       }                                                                      \
    }                                                                         \
    else                                                                      \
    {                                                                         \
       json = vaa_to_json((void *)_proto_msg);                                \
       if (!json)                                                             \
       {                                                                      \
           DBG1(DBG_KNL, "failed to convert msg with key %s to JSON!", key);  \
           return FAILED;                                                     \
       }                                                                      \
       void *reply = redisCommand(this->redis_context,                        \
               "set %s %s", key, json);                                       \
       if (NULL == reply)                                                     \
       {                                                                      \
           DBG1(DBG_KNL, "error on set command: %s",                          \
                   this->redis_context->errstr);                              \
           free(json);                                                        \
           return FAILED;                                                     \
       }                                                                      \
       free(json);                                                            \
    }                                                                         \
} while (0)

typedef struct private_vac_t private_vac_t;

/**
 * VPP-agent client
 */
vac_t *vac;

/* common dump request message */
static Configurator__DumpRequest dump_request =
        CONFIGURATOR__DUMP_REQUEST__INIT;

/**
 * Private variables and functions of vac_t class.
 */
struct private_vac_t {

    /**
     * public part of the vac_t object.
     */
    vac_t public;

    char * (*to_json)(void *msg,
            const ProtobufCMessageDescriptor *desc);

    void *(*to_proto)(char *msg,
            const ProtobufCMessageDescriptor *desc);

    grpc_c_client_t *grpc_client;
    redisContext *redis_context;

    const char *grpc_host;
    const char *redis_host;
    const char *channel;
    const char *vpp_label;
};

#define DEFINE_COMMON_PROTOS \
    Vpp__ConfigData data = VPP__CONFIG_DATA__INIT;                            \
    Configurator__UpdateRequest upd_rq = CONFIGURATOR__UPDATE_REQUEST__INIT;  \
    Configurator__DeleteRequest del_rq = CONFIGURATOR__DELETE_REQUEST__INIT;  \
    Configurator__Config config_data = CONFIGURATOR__CONFIG__INIT;            \
    upd_rq.update = &config_data;                                             \
    del_rq.delete_ = &config_data;                                            \
    config_data.vpp_config = &data;                                           \
    Configurator__UpdateResponse *upd_rp = NULL;                              \
    Configurator__DeleteResponse *del_rp = NULL;

METHOD(vac_t, vac_update_vpp_interface, status_t, private_vac_t *this,
        Vpp__Interfaces__Interface *iface, int is_add)
{
    status_t rc;
    if (this->public.is_grpc_channel)
    {
        DEFINE_COMMON_PROTOS;

        Vpp__Interfaces__Interface *tunnels[1];
        data.interfaces = tunnels;
        data.interfaces[0] = iface;
        data.n_interfaces = 1;

        if (is_add)
            rc = rpc_call(&upd_rq, &upd_rp, update);
        else
            rc = rpc_call(&del_rq, &del_rp, delete);

        if (SUCCESS != rc)
            return FAILED;

        if (is_add)
            configurator__update_response__free_unpacked(upd_rp, 0);
        else
            configurator__delete_response__free_unpacked(del_rp, 0);
    }
    else
    {
        redis_update(iface, is_add, REDIS_KEY_VPP_INTERFACE,
                this->vpp_label, iface->name);
    }

    return SUCCESS;
}

METHOD(vac_t, vac_update_punt_socket, status_t, private_vac_t *this,
        Vpp__Punt__ToHost *punt, int is_add)
{
    status_t rc;
    if (this->public.is_grpc_channel)
    {
        DEFINE_COMMON_PROTOS;

        Vpp__Punt__ToHost *punts[1];
        data.n_punt_tohosts = 1;
        data.punt_tohosts = punts;
        data.punt_tohosts[0] = punt;

        if (is_add)
            rc = rpc_call(&upd_rq, &upd_rp, update);
        else
            rc = rpc_call(&del_rq, &del_rp, delete);

        if (SUCCESS != rc)
            return FAILED;

        if (is_add)
            configurator__update_response__free_unpacked(upd_rp, 0);
        else
            configurator__delete_response__free_unpacked(del_rp, 0);
    }
    else
    {
        redis_update(punt, is_add, REDIS_KEY_TOHOST, this->vpp_label,
                punt->l3_protocol, punt->l4_protocol, punt->port);
    }

    return SUCCESS;
}

METHOD(vac_t, vac_update_route, status_t, private_vac_t *this,
        Vpp__L3__Route *route, int is_add)
{
    status_t rc;
    if (this->public.is_grpc_channel)
    {
        DEFINE_COMMON_PROTOS;

        Vpp__L3__Route *routes;
        data.n_routes = 1;
        data.routes = &routes;
        data.routes[0] = route;

        if (is_add)
            rc = rpc_call(&upd_rq, &upd_rp, update);
        else
            rc = rpc_call(&del_rq, &del_rp, delete);

        if (SUCCESS != rc)
            return FAILED;

        if (is_add)
            configurator__update_response__free_unpacked(upd_rp, 0);
        else
            configurator__delete_response__free_unpacked(del_rp, 0);
    }
    else
    {
        redis_update(route, is_add, REDIS_KEY_ROUTE, this->vpp_label,
                route->dst_network, route->next_hop_addr);
    }

    return SUCCESS;
}

static status_t
redis_get_value(redisContext *ctx, const char *key,
        const ProtobufCMessageDescriptor *desc, void **out)
{
    redisReply *reply = redisCommand(ctx, "get %s", key);
    if (NULL == reply)
    {
        DBG1(DBG_KNL, "kernel_vpp: failed to get key: %s", key);
        return FAILED;
    }

    *out = vaa_to_proto(reply->str, desc);
    if (*out == NULL)
    {
        freeReplyObject(reply);
        DBG1(DBG_KNL, "kernel_vpp: failed to convert JSON to proto, key: %s",
                key);
        return FAILED;
    }
    freeReplyObject(reply);
    return SUCCESS;
}

static status_t
process_keys(redisContext *ctx, redisReply *reply,
        const ProtobufCMessageDescriptor *desc, size_t *n, void ***out)
{
    status_t rc;
    size_t i, new_size = *n + reply->elements;
    void *tmp = realloc(*out, new_size * sizeof(void *));
    if (!tmp)
    {
        return FAILED;
    }
    *out = tmp;

    for (i = 0; i < reply->elements; i++)
    {
        rc = redis_get_value(ctx, reply->element[i]->str, desc, *out + *n);
        if (rc != SUCCESS)
            return FAILED;
        (*n)++;
    }
    return SUCCESS;
}

void free_proto_array(void **msg, size_t n)
{
    size_t i;
    for (i = 0; i < n; i++)
    {
        protobuf_c_message_free_unpacked((ProtobufCMessage*)msg[i], 0);
    }
    free(msg);
}

#define redis_dump(_fmt, _desc, _args...)                                     \
do {                                                                          \
    *rp = NULL;                                                               \
    char *iterator = strdup("0");                                             \
    do {                                                                      \
        redisReply *reply = redisCommand(this->redis_context, _fmt,           \
            iterator, ##_args);                                               \
        if (reply->type != REDIS_REPLY_ARRAY)                                 \
            return FAILED;                                                    \
        if (reply->elements < 2)                                              \
            return FAILED;                                                    \
        free(iterator);                                                       \
        iterator = strdup(reply->element[0]->str);                            \
        rc = process_keys(this->redis_context, reply->element[1],             \
                _desc, n, (void ***)rp);                                      \
        if (rc != SUCCESS) {                                                  \
            free_proto_array((void **)*rp, *n);                               \
            break;                                                            \
        }                                                                     \
        freeReplyObject(reply);                                               \
    } while (strcmp(iterator, "0"));                                          \
    if (iterator) free(iterator);                                             \
    rc = SUCCESS;                                                             \
} while (0)

METHOD(vac_t, vac_dump_interfaces, status_t, private_vac_t *this,
       Vpp__Interfaces__Interface ***rp, size_t *n)
{
    status_t rc;
    Configurator__DumpResponse *dump_rsp = NULL;
    *n = 0;

    if (this->public.is_grpc_channel)
    {
        rc = rpc_call(&dump_request, &dump_rsp, dump);
        if (rc == SUCCESS)
        {
            if (dump_rsp && dump_rsp->dump && dump_rsp->dump->vpp_config)
            {
                *rp = dump_rsp->dump->vpp_config->interfaces;
                *n = dump_rsp->dump->vpp_config->n_interfaces;
                dump_rsp->dump->vpp_config->interfaces = NULL;
                dump_rsp->dump->vpp_config->n_interfaces = 0;
            }
            configurator__dump_response__free_unpacked(dump_rsp, 0);
        }
    }
    else
    {
        redis_dump(REDIS_VPP_INTERFACE_DUMP,
                &vpp__interfaces__interface__descriptor, this->vpp_label);
    }
    return rc;
}

METHOD(vac_t, vac_dump_routes, status_t, private_vac_t *this,
        Vpp__L3__Route ***rp, size_t *n)
{
    status_t rc;
    Configurator__DumpResponse *dump_rsp = NULL;
    *n = 0;

    if (this->public.is_grpc_channel)
    {
        rc = rpc_call(&dump_request, &dump_rsp, dump);
        if (rc == SUCCESS)
        {
            if (dump_rsp && dump_rsp->dump && dump_rsp->dump->vpp_config)
            {
                *rp = dump_rsp->dump->vpp_config->routes;
                *n = dump_rsp->dump->vpp_config->n_routes;
                dump_rsp->dump->vpp_config->routes = NULL;
                dump_rsp->dump->vpp_config->n_routes = 0;
            }
            configurator__dump_response__free_unpacked(dump_rsp, 0);
        }
    }
    else
    {
        redis_dump(REDIS_ROUTE_DUMP, &vpp__l3__route__descriptor,
                this->vpp_label);
    }
    return rc;
}

METHOD(vac_t,
       vac_register_events, status_t,
       private_vac_t *this,
       Configurator__NotificationRequest *rq,
       grpc_c_client_callback_t *cb,
       void *tag)
{
    if (!this->public.is_grpc_channel)
    {
        DBG1(DBG_KNL, "kernel_vpp: notifications only supported for "
                "grpc channel!");
        return FAILED;
    }

    int rpc_status = configurator__configurator__notify__async(
            this->grpc_client,
            NULL, /* metadata array */
            0, /* flags */
            rq,
            cb,
            tag);
    return rpc_status ? FAILED : SUCCESS;
}

METHOD(vac_t, destroy, void, private_vac_t *this)
{
    grpc_c_client_free(this->grpc_client);
}

METHOD(vac_t, vac_dump_punts, status_t, private_vac_t *this,
        Vpp__Punt__ToHost ***rp, size_t *n)
{
    status_t rc = FAILED;
    Configurator__DumpResponse *dump_rsp = NULL;
    *n = 0;

    if (this->public.is_grpc_channel)
    {
        rc = rpc_call(&dump_request, &dump_rsp, dump);
        if (rc == SUCCESS)
        {
            if (dump_rsp && dump_rsp->dump && dump_rsp->dump->vpp_config)
            {
                *rp = dump_rsp->dump->vpp_config->punt_tohosts;
                *n = dump_rsp->dump->vpp_config->n_punt_tohosts;
                dump_rsp->dump->vpp_config->punt_tohosts = NULL;
                dump_rsp->dump->vpp_config->n_punt_tohosts = 0;
            }
            configurator__dump_response__free_unpacked(dump_rsp, 0);
        }
    }
    else
    {
        redis_dump(REDIS_PUNTS_DUMP, &vpp__punt__to_host__descriptor,
                this->vpp_label);
    }
    return rc;
}

vac_t *vac_create(char *name)
{
    private_vac_t *this;

    INIT(this,
        .public = {
            .destroy = _destroy,
            .dump_punts = _vac_dump_punts,
            .dump_interfaces = _vac_dump_interfaces,
            .dump_routes = _vac_dump_routes,
            .register_events = _vac_register_events,
            .update_vpp_interface = _vac_update_vpp_interface,
            .update_punt_socket = _vac_update_punt_socket,
            .update_route = _vac_update_route,
            .is_grpc_channel = 1,
        },
        .grpc_host = lib->settings->get_str(lib->settings,
            "%s.plugins.kernel-vpp.grpc",
            VPP_AGENT_DEFAULT_GRPC_HOST, lib->ns),
        .redis_host = lib->settings->get_str(lib->settings,
            "%s.plugins.kernel-vpp.redis",
            VPP_AGENT_DEFAULT_REDIS_HOST, lib->ns),
        .channel = lib->settings->get_str(lib->settings,
            "%s.plugins.kernel-vpp.channel",
            VPP_AGENT_DEFAULT_CHANNEL, lib->ns),
        .vpp_label = lib->settings->get_str(lib->settings,
            "%s.plugins.kernel-vpp.vpp_label",
            VPP_AGENT_DEFAULT_VPP_LABEL, lib->ns),
    );

    if (!strcmp(this->channel, "redis"))
    {
        this->public.is_grpc_channel = 0;
    }
    else if (!strcmp(this->channel, VPP_AGENT_DEFAULT_CHANNEL))
    {
        this->public.is_grpc_channel = 1;
    }
    else
    {
        DBG1(DBG_KNL, "kernel_vpp: unknown channel name '%s', fallback to grpc",
                this->grpc_host);
    }

    if (this->public.is_grpc_channel)
    {
        DBG1(DBG_KNL, "kernel_vpp: Connecting to gRPC at %s", this->grpc_host);

        grpc_c_init(GRPC_THREADS, NULL);
        this->grpc_client = grpc_c_client_init_by_host(this->grpc_host,
                name, NULL, NULL);
        if (!this->grpc_client)
        {
            DBG1(DBG_KNL, "kernel_vpp: cannot connect to gRPC host: %s!",
                    this->grpc_host);
            return NULL;
        }
    }
    else
    {
        char *host = strdup(this->redis_host);
        char *port = strchr(host, ':');
        if (!port)
        {
            DBG1(DBG_KNL, "kernel_vpp: Invalid format of host: %s!", host);
            free(host);
            return NULL;
        }
        port[0] = '\0';
        port++;
        int redis_port = atoi(port);

        DBG1(DBG_KNL, "kernel_vpp: Connecting to redis at %s:%d",
                host, redis_port);
        this->redis_context = redisConnect(host, redis_port);
        if (this->redis_context == NULL || this->redis_context->err) {
            if (this->redis_context) {
                DBG1(DBG_KNL, "Error: %s", this->redis_context->errstr);
            } else {
                DBG1(DBG_KNL, "Can't allocate redis context!");
            }
            free(host);
            return NULL;
        }
        free(host);
    }

    vac = &this->public;
    return &this->public;
}
