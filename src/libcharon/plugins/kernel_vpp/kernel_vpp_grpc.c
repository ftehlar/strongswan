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
#include <collections/array.h>
#include <collections/hashtable.h>

#include "api/kiknos_api_wrapper.h"
#include "kernel_vpp_grpc.h"

#define VPP_AGENT_DEFAULT_GRPC_HOST "localhost:9111"

typedef struct private_vac_t private_vac_t;

/**
 * VPP-agent client
 */
vac_t *vac;

/**
 * Private variables and functions of vac_t class.
 */
struct private_vac_t {

    /**
     * public part of the vac_t object.
     */
    vac_t public;

    const char *grpc_host;
};

METHOD(vac_t, destroy, void, private_vac_t *this)
{
    // TODO
}

METHOD(vac_t, add_punt_sockets, kiknos_rc_t, private_vac_t *this,
        kiknos_punt_t *punts, int count)
{
    return kiknos_add_punt_sockets(punts, count);
}

METHOD(vac_t, get_agent_punt_socket, char *, private_vac_t *this)
{
    return kiknos_get_agent_punt_socket();
}

METHOD(vac_t, get_if_name_by_ip, char *, private_vac_t *this, char *ip)
{
    return kiknos_get_if_name_by_ip(ip);
}

METHOD(vac_t, update_route, kiknos_rc_t, private_vac_t *this,
        kiknos_route_t *route, int is_add)
{
    return is_add ? kiknos_add_route(route) : kiknos_del_route(route);
}

METHOD(vac_t, update_tunnel, kiknos_rc_t, private_vac_t *this,
        kiknos_ipsec_tunnel_t *tun, int is_add)
{
    return is_add ? kiknos_add_tunnel(tun) : kiknos_del_tunnel(tun);
}

vac_t *vac_create(char *name)
{
    private_vac_t *this;

    INIT(this,
        .public = {
            .destroy = _destroy,
            .add_punt_sockets = _add_punt_sockets,
            .get_agent_punt_socket = _get_agent_punt_socket,
            .get_if_name_by_ip = _get_if_name_by_ip,
            .update_route = _update_route,
            .update_tunnel = _update_tunnel,
        },
        .grpc_host = lib->settings->get_str(lib->settings,
            "%s.plugins.kernel-vpp.grpc",
            VPP_AGENT_DEFAULT_GRPC_HOST, lib->ns),
    );

    DBG1(DBG_KNL, "kernel_vpp: initializing gRPC: %s", this->grpc_host);

    /* init grpc client library */
    if (KIKNOS_RC_OK != kiknos_client_create(this->grpc_host)) {
        DBG1(DBG_KNL, "kernel_vpp: gRPC init failed!");
        return NULL;
    }

    vac = &this->public;
    return &this->public;
}
