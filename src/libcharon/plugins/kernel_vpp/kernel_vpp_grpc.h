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
#ifndef KERNEL_VPP_GRPC_H_
#define KERNEL_VPP_GRPC_H_

#include "api/kiknos_api_wrapper.h"

typedef struct vac_t vac_t;

/**
 * Wrapper around Kiknos agent client
 */
struct vac_t {
    void (*destroy)(vac_t *this);

    kiknos_rc_t (*add_punt_sockets)(vac_t *this, kiknos_punt_t *punts, int count);
    char * (*get_agent_punt_socket)(vac_t *this);
    char * (*get_if_name_by_ip)(vac_t *this, char *ip);
    kiknos_rc_t (*update_route)(vac_t *this, kiknos_route_t *route, int is_add);
    kiknos_rc_t (*update_tunnel)(vac_t *this, kiknos_ipsec_tunnel_t *tun,
            int is_add);
};

extern vac_t *vac;

vac_t *vac_create(char *name);

#endif /* KERNEL_VPP_GRPC_H_ */
