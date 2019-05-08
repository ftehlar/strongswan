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
#include <unistd.h>
#include <utils/chunk.h>
#include <utils/debug.h>
#include <threading/thread.h>

#include "api/kiknos_api_wrapper.h"
#include "kernel_vpp_net.h"
#include "kernel_vpp_grpc.h"

typedef struct private_kernel_vpp_net_t private_kernel_vpp_net_t;

/**
 * Private data of kernel_vpp_net implementation.
 */
struct private_kernel_vpp_net_t {

    /**
     * Public interface.
     */
    kernel_vpp_net_t public;
};

/**
 * Add or remove a route
 */
static status_t manage_route(private_kernel_vpp_net_t *this, bool add,
                             chunk_t dst, uint8_t prefixlen, host_t *gtw,
                             char *name)
{
    kiknos_rc_t rc;
    host_t *dst_ip_addr;
    int family;
    char ippref[128];
    kiknos_route_t route;

    memset(&route, 0, sizeof(route));

    if (dst.len == 4)
    {
        family = AF_INET;
    }
    else if (dst.len == 16)
    {
        family = AF_INET6;
    }
    else
    {
        DBG1(DBG_KNL, "cannot determine IP family (length = %d)!", dst.len);
        return FAILED;
    }

    dst_ip_addr = host_create_from_chunk(family, dst, 0);
    if (!dst_ip_addr)
    {
        DBG1(DBG_KNL, "cannot build host address!");
        return FAILED;
    }

    route.outgoing_interface = name;
    if (snprintf(ippref, sizeof(ippref), "%H/%d", dst_ip_addr, prefixlen)
            >= sizeof(ippref))
    {
        return FAILED;
    }
    route.dst_net = ippref;
    dst_ip_addr->destroy(dst_ip_addr);

    if (gtw)
    {
        char nh_addr[INET6_ADDRSTRLEN];
        if (snprintf(nh_addr, sizeof(nh_addr), "%H", gtw) >= sizeof(nh_addr))
        {
            return FAILED;
        }
        route.next_hop = nh_addr;
    }

    rc = vac->update_route(vac, &route, add);
    if (rc != KIKNOS_RC_OK)
    {
        DBG1(DBG_KNL, "vac %sing route failed", add ? "add" : "remov");
        return FAILED;
    }

    return SUCCESS;
}

METHOD(kernel_net_t, get_interface_name, bool,
    private_kernel_vpp_net_t *this, host_t* ip, char **name)
{
    char *ret;
    int found = 0;
    char ip_str[INET6_ADDRSTRLEN] = {0, };

    snprintf(ip_str, sizeof(ip_str), "%H", ip);
    ret = vac->get_if_name_by_ip(vac, ip_str);
    found = ret != NULL;

    if (name) {
        *name = ret;
    } else if (ret) {
        free(ret);
    }

    return found;
}

METHOD(kernel_net_t, get_source_addr, host_t*,
    private_kernel_vpp_net_t *this, host_t *dest, host_t *src)
{
    DBG1(DBG_KNL, "get_source_addr unsupported!");
    return NULL;
}

METHOD(kernel_net_t, get_nexthop, host_t*,
    private_kernel_vpp_net_t *this, host_t *dest, int prefix, host_t *src,
    char **iface)
{
    DBG1(DBG_KNL, "get_nexthop unsupported!");
    return NULL;
}

METHOD(kernel_net_t, add_ip, status_t,
    private_kernel_vpp_net_t *this, host_t *virtual_ip, int prefix,
    char *iface_name)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_net_t, del_ip, status_t,
    private_kernel_vpp_net_t *this, host_t *virtual_ip, int prefix,
    bool wait)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_net_t, add_route, status_t,
    private_kernel_vpp_net_t *this, chunk_t dst_net, u_int8_t prefixlen,
    host_t *gateway, host_t *src_ip, char *if_name)
{
    return manage_route(this, TRUE, dst_net, prefixlen, gateway, if_name);
}

METHOD(kernel_net_t, del_route, status_t,
    private_kernel_vpp_net_t *this, chunk_t dst_net, u_int8_t prefixlen,
    host_t *gateway, host_t *src_ip, char *if_name)
{
    return manage_route(this, FALSE, dst_net, prefixlen, gateway, if_name);
}

METHOD(kernel_net_t, destroy, void,
    private_kernel_vpp_net_t *this)
{
    free(this);
}

kernel_vpp_net_t *kernel_vpp_net_create()
{
    private_kernel_vpp_net_t *this;

    INIT(this,
        .public = {
            .interface = {
                .get_interface = _get_interface_name,
                .get_source_addr = _get_source_addr,
                .get_nexthop = _get_nexthop,
                .add_ip = _add_ip,
                .del_ip = _del_ip,
                .add_route = _add_route,
                .del_route = _del_route,
                .destroy = _destroy,
            },
        },
    );

    return &this->public;
}
