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
#include "hiredis/hiredis.h"
#include "vaa_json.h"

#define REDIS_IP "127.0.0.1"
#define REDIS_PORT 6380
#define VPP_AGENT_HOST "127.0.0.1:9111"
#define KEY_PFX "/vnf-agent/vpp1"

int configure_vpp_agent(Configurator__UpdateRequest *rq)
{
    grpc_c_client_t *client;
    grpc_c_init(GRPC_THREADS, NULL);
    Configurator__UpdateResponse *rsp = NULL;

    client = grpc_c_client_init_by_host(VPP_AGENT_HOST, "test_setup", NULL,
            NULL);

    int rpc_status = configurator__configurator__update(client,
            NULL, /* metadata array */
            0, /* flags */
            rq,
            &rsp,
            NULL /* status */,
            -1 /* timeout */);

    printf("vpp_agent result: %d\n", rpc_status);
    configurator__update_response__free_unpacked(rsp, 0);
    return rpc_status;
}

static int set_item(redisContext *c, void *proto, char *key)
{
    char *msg = vaa_to_json(proto);
    if (!msg) {
        fprintf(stderr, "failed to convert interface0!\n");
        return 1;
    }
    void *reply = redisCommand(c, "set %s %s", key, msg);
    if (NULL == reply) {
        printf("error executing command: %s\n", c->errstr);
        return 1;
    }
    printf("-- key set: %s\n  %s\n", key, msg);
    free(msg);
    return 0;
}

int configure_redis(Configurator__UpdateRequest *rq)
{
    redisContext *c = redisConnect(REDIS_IP, REDIS_PORT);

    if (c == NULL || c->err) {
        if (c) {
            printf("Error: %s\n", c->errstr);
        } else {
            printf("Can't allocate redis context\n");
        }
        return 1;
    }
    if (set_item(c, (void *)rq->update->linux_config->interfaces[0],
                KEY_PFX "/config/linux/interfaces/v2/interface/wan0")) {
        return 1;
    }
    if (set_item(c, (void *)rq->update->linux_config->interfaces[1],
                KEY_PFX "/config/linux/interfaces/v2/interface/wan1")) {
        return 1;
    }
    if (set_item(c, (void *)rq->update->vpp_config->interfaces[0],
                KEY_PFX "/config/vpp/v2/interfaces/tap0")) {
        return 1;
    }
    if (set_item(c, (void *)rq->update->vpp_config->interfaces[1],
                KEY_PFX "/config/vpp/v2/interfaces/wan0")) {
        return 1;
    }

    return 0;
}

int main(int argc, char *argv[])
{
    int rc = EXIT_FAILURE;
    Configurator__UpdateRequest rq = CONFIGURATOR__UPDATE_REQUEST__INIT;

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
    Configurator__Config data = CONFIGURATOR__CONFIG__INIT;
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

    if (argc > 1 && !strcmp(argv[1], "--use-redis")) {
        printf("info: using redis %s:%d\n", REDIS_IP, REDIS_PORT);
        rc = configure_redis(&rq);
    } else {
        printf("info: using vpp-agent %s\n", VPP_AGENT_HOST);
        rc = configure_vpp_agent(&rq);
    }

    return !rc ? EXIT_SUCCESS : EXIT_FAILURE;
}
