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

#define VPP_AGENT_DEFAULT_HOST "localhost:9111"

typedef struct private_vac_t private_vac_t;

/**
 * VPP-agent client (gRPC based)
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

    grpc_c_client_t *grpc_client;

    const char *host;
};

METHOD(vac_t, vac_put, status_t, private_vac_t *this,
        Vpp__ConfigData *data, Configurator__UpdateResponse **rp)
{
    Configurator__UpdateRequest rq = CONFIGURATOR__UPDATE_REQUEST__INIT;
    Configurator__Config config_data = CONFIGURATOR__CONFIG__INIT;
    config_data.vpp_config = data;
    rq.update = &config_data;

    int rpc_status = configurator__configurator__update(
            this->grpc_client,
            NULL, /* metadata array */
            0, /* flags */
            &rq,
            rp,
            NULL /* status, ignored due to vpp-agent not filling it */,
            -1 /* timeout */);

    return rpc_status ? FAILED : SUCCESS;
}

METHOD(vac_t, vac_del, status_t, private_vac_t *this,
        Vpp__ConfigData *data, Configurator__DeleteResponse **rp)
{
    Configurator__DeleteRequest rq = CONFIGURATOR__DELETE_REQUEST__INIT;
    Configurator__Config config_data = CONFIGURATOR__CONFIG__INIT;
    config_data.vpp_config = data;
    rq.delete_ = &config_data;

    int rpc_status = configurator__configurator__delete (
            this->grpc_client,
            NULL, /* metadata array */
            0, /* flags */
            &rq,
            rp,
            NULL /* status, ignored due to vpp-agent not filling it */,
            -1 /* timeout */);

    return rpc_status ? FAILED : SUCCESS;
}

METHOD(vac_t, vac_dump_interfaces, status_t, private_vac_t *this,
        Configurator__DumpResponse **rp)
{
    int rpc_status = configurator__configurator__dump(
            this->grpc_client,
            NULL, /* metadata array */
            0, /* flags */
            &dump_request,
            rp,
            NULL /* status, ignored due to vpp-agent not filling it */,
            -1 /* timeout */);
    return rpc_status ? FAILED : SUCCESS;
}

METHOD(vac_t, vac_dump_routes, status_t, private_vac_t *this,
        Configurator__DumpResponse **rp)
{
    int rpc_status = configurator__configurator__dump(
            this->grpc_client,
            NULL, /* metadata array */
            0, /* flags */
            &dump_request,
            rp,
            NULL /* status, ignored due to vpp-agent not filling it */,
            -1 /* timeout */);
    return rpc_status ? FAILED : SUCCESS;
}

METHOD(vac_t,
       vac_register_events, status_t,
       private_vac_t *this,
       Configurator__NotificationRequest *rq,
       grpc_c_client_callback_t *cb,
       void *tag)
{
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
        Configurator__DumpResponse **rp)
{
    int rpc_status = configurator__configurator__dump(
            this->grpc_client,
            NULL, /* metadata array */
            0, /* flags */
            &dump_request,
            rp,
            NULL /* status, ignored due to vpp-agent not filling it */,
            -1 /* timeout */);
    return rpc_status ? FAILED : SUCCESS;
}

vac_t *vac_create(char *name)
{
    private_vac_t *this;

    INIT(this,
        .public = {
            .put = _vac_put,
            .del = _vac_del,
            .destroy = _destroy,
            .dump_punts = _vac_dump_punts,
            .dump_interfaces = _vac_dump_interfaces,
            .dump_routes = _vac_dump_routes,
            .register_events = _vac_register_events,
        },
        .host = lib->settings->get_str(lib->settings,
            "%s.plugins.kernel-vpp.host",
            VPP_AGENT_DEFAULT_HOST, lib->ns),
    );

    grpc_c_init(GRPC_THREADS, NULL);
    this->grpc_client = grpc_c_client_init_by_host(this->host,
            name, NULL, NULL);

    if (!this->grpc_client)
    {
        DBG1(DBG_KNL, "cannot connect to gRPC host: %s!", this->host);
        return NULL;
    }

    vac = &this->public;
    return &this->public;
}
