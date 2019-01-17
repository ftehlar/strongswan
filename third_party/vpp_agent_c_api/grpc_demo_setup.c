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
#include "configurator/configurator.grpc-c.h"

int main()
{
    grpc_c_client_t *client;
    grpc_c_init(GRPC_THREADS, NULL);

    client = grpc_c_client_init_by_host("127.0.0.1:9111", "strongswan", NULL,
            NULL);

    Dataconfigurator__UpdateRequest rq = DATACONFIGURATOR__UPDATE_REQUEST__INIT;
    Dataconfigurator__UpdateResponse *rsp = NULL;
    Linux__Interfaces__Interface *wans[2];
    Linux__Interfaces__Interface wan0 = LINUX__INTERFACES__INTERFACE__INIT;
    Linux__Interfaces__Interface wan1 = LINUX__INTERFACES__INTERFACE__INIT;

    Linux__Interfaces__VethLink veth_wan0 = LINUX__INTERFACES__VETH_LINK__INIT;
    veth_wan0.peer_if_name = "wan1";
    Linux__Interfaces__VethLink veth_wan1 = LINUX__INTERFACES__VETH_LINK__INIT;
    veth_wan1.peer_if_name = "wan0";

    Linux__ConfigData linux_data = LINUX__CONFIG_DATA__INIT;
    linux_data.n_interfaces = 2;
    linux_data.interfaces = wans;
    linux_data.interfaces[0] = &wan0;
    linux_data.interfaces[1] = &wan1;

    wan0.name = "wan0";
    wan0.has_type = 1;
    wan0.type = LINUX__INTERFACES__INTERFACE__TYPE__VETH;
    wan0.veth = &veth_wan0;
    wan0.link_case = LINUX__INTERFACES__INTERFACE__LINK_VETH;
    wan0.has_enabled = 1;
    wan0.enabled = 1;

    wan1.name = "wan1";
    wan1.has_type = 1;
    wan1.type = LINUX__INTERFACES__INTERFACE__TYPE__VETH;
    wan1.veth = &veth_wan1;
    wan1.link_case = LINUX__INTERFACES__INTERFACE__LINK_VETH;
    wan1.has_enabled = 1;
    wan1.enabled = 1;

    Vpp__ConfigData vpp_data = VPP__CONFIG_DATA__INIT;
    Dataconfigurator__Config data = DATACONFIGURATOR__CONFIG__INIT;
    data.vpp_config = &vpp_data;
    data.linux_config = &linux_data;
    rq.update = &data;
    rq.has_full_resync = 1;
    rq.full_resync = 1;

    char *tap_ips, *af_ips;
    Vpp__Interfaces__Interface *ifs[2];

    Vpp__Interfaces__Interface tap = VPP__INTERFACES__INTERFACE__INIT;
    tap.name = "tap0";
    tap.has_enabled = 1;
    tap.enabled = 1;
    tap.has_type = 1;
    tap.type = VPP__INTERFACES__INTERFACE__TYPE__TAP;

    tap.tap = calloc(1, sizeof(Vpp__Interfaces__TapLink));
    vpp__interfaces__tap_link__init(tap.tap);
    tap.tap->host_if_name = "tap0";

    tap.n_ip_addresses = 1;
    tap.ip_addresses = &tap_ips;
    tap.ip_addresses[0] = "10.10.10.1/24";

    Vpp__Interfaces__Interface af = VPP__INTERFACES__INTERFACE__INIT;
    af.name = "wan0";
    af.has_type = 1;
    af.type = VPP__INTERFACES__INTERFACE__TYPE__AF_PACKET;
    af.has_enabled = 1;
    af.enabled = 1;
    af.n_ip_addresses = 1;
    af.ip_addresses = &af_ips;
    af.ip_addresses[0] = "172.16.0.2/24";

    af.link_case = VPP__INTERFACES__INTERFACE__LINK_AFPACKET;
    Vpp__Interfaces__AfpacketLink af_data =
        VPP__INTERFACES__AFPACKET_LINK__INIT;

    af.afpacket = &af_data;
    af_data.host_if_name = "wan0";

    vpp_data.n_interfaces = 2;
    vpp_data.interfaces = ifs;
    vpp_data.interfaces[0] = &tap;
    vpp_data.interfaces[1] = &af;

    int rpc_status = dataconfigurator__configurator__update(client,
            NULL, /* metadata array */
            0, /* flags */
            &rq,
            &rsp,
            NULL /* status */,
            -1 /* timeout */);

    printf("rc: %d\n", rpc_status);

    return rpc_status;
}
