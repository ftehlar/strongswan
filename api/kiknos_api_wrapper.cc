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

#include <grpc/grpc.h>
#include <grpc++/channel.h>
#include <grpc++/client_context.h>
#include <grpc++/create_channel.h>

#include "ssipsec.grpc.pb.h"
#include "kiknos_api_wrapper.h"

extern "C" {

using grpc::Status;
using grpc::ClientContext;
using model::SSipSec;
using model::InterfaceName;
using model::ReturnCode;
using model::IPAddress;
using model::AgentPuntSktRqst;
using model::AgentPuntSocket;
using model::Response;
using model::SswanPuntSockets;
using model::SswanPuntSocket;
using model::IntegAlg;
using model::CryptoAlg;
using model::Route;
using model::IPSecTunnel;

std::unique_ptr<SSipSec::Stub> stub;

kiknos_rc_t kiknos_client_create (const char *host)
{
    stub = SSipSec::NewStub(grpc::CreateChannel(host,
                            grpc::InsecureChannelCredentials()));
    if (!stub) {
        return KIKNOS_RC_FAIL;
    }
    return KIKNOS_RC_OK;
}

static kiknos_rc_t
convert_rc (ReturnCode rc)
{
    switch (rc) {
        case ReturnCode::OK: return KIKNOS_RC_OK;
        case ReturnCode::FAIL: return KIKNOS_RC_FAIL;
    };
    return KIKNOS_RC_FAIL;
}

static kiknos_rc_t
add_del_route (kiknos_route_t *route, bool is_add)
{
    ClientContext ctx;
    Response rsp;
    Status status;
    Route r;

    r.set_dst_network(route->dst_net);
    r.set_next_hop_addr(route->next_hop);
    r.set_outgoing_interface(route->outgoing_interface);
    r.set_preference(route->preference);

    if (is_add) {
        status = stub->AddRoute(&ctx, r, &rsp);
    } else {
        status = stub->DelRoute(&ctx, r, &rsp);
    }

    if (!status.ok()) {
        return KIKNOS_RC_FAIL;
    }
    return convert_rc(rsp.rc());
}

kiknos_rc_t
kiknos_add_route (kiknos_route_t *route)
{
    return add_del_route(route, true);
}

kiknos_rc_t
kiknos_del_route (kiknos_route_t *route)
{
    return add_del_route(route, false);
}

static kiknos_rc_t
add_del_tunnel (kiknos_ipsec_tunnel_t *tun, bool is_add)
{
    ClientContext ctx;
    Response rsp;
    Status status;
    IPSecTunnel t;

    t.set_name(tun->name);
    t.set_esn(tun->esn);
    t.set_local_ip(tun->local_ip);
    t.set_remote_ip(tun->remote_ip);
    t.set_local_spi(tun->local_spi);
    t.set_remote_spi(tun->remote_spi);

    t.set_crypto_alg((CryptoAlg)tun->crypto_alg);
    t.set_local_crypto_key(tun->local_crypto_key);
    t.set_remote_crypto_key(tun->remote_crypto_key);

    t.set_integ_alg((IntegAlg)tun->integ_alg);
    t.set_local_integ_key(tun->local_integ_key);
    t.set_remote_integ_key(tun->remote_integ_key);

    t.set_enable_udp_encap(tun->enable_udp_encap);
    t.set_interface_with_ip(tun->interface_with_ip);

    if (is_add) {
        status = stub->AddTunnel(&ctx, t, &rsp);
    } else {
        status = stub->DelTunnel(&ctx, t, &rsp);
    }

    if (!status.ok()) {
        return KIKNOS_RC_FAIL;
    }
    return convert_rc(rsp.rc());
}

kiknos_rc_t
kiknos_add_tunnel (kiknos_ipsec_tunnel_t *tun)
{
    return add_del_tunnel(tun, true);
}

kiknos_rc_t
kiknos_del_tunnel (kiknos_ipsec_tunnel_t *tun)
{
    return add_del_tunnel(tun, false);
}

kiknos_rc_t
kiknos_add_punt_sockets (kiknos_punt_t *punts, int count)
{
    ClientContext ctx;
    Response rsp;
    Status status;
    SswanPuntSockets rq;
    SswanPuntSocket *item;

    while (count > 0) {
        count--;
        item = rq.add_punt_sockets();
        item->set_port(punts[count].port);
        item->set_socket_path(punts[count].socket_path);
    }

    status = stub->SetSswanPuntSockets(&ctx, rq, &rsp);
    if (!status.ok()) {
        return KIKNOS_RC_FAIL;
    }
    return convert_rc(rsp.rc());
}

char *
kiknos_get_agent_punt_socket (void)
{
    ClientContext ctx;
    AgentPuntSktRqst rq;
    AgentPuntSocket rsp;

    Status status = stub->GetAgentPuntSocket(&ctx, rq, &rsp);
    if (!status.ok()) {
        return NULL;
    }
    return strdup(rsp.socket_path().c_str());
}

char *
kiknos_get_if_name_by_ip (char *ip)
{
    ClientContext ctx;
    InterfaceName if_name;
    IPAddress ip_addr;

    ip_addr.set_ip(ip);

    Status status = stub->GetIfNameByIP(&ctx, ip_addr, &if_name);
    if (!status.ok()) {
        return NULL;
    }
    return strdup(if_name.name().c_str());
}

} /* extern "C" */
