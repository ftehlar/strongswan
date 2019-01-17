#ifndef KERNEL_VPP_GRPC_H_
#define KERNEL_VPP_GRPC_H_
/*
 * Copyright (c) 2018 Cisco and/or its affiliates.
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

#include "configurator/configurator.grpc-c.h"
#include "models/vpp/vpp.grpc-c.h"

typedef struct vac_t vac_t;

/**
 * Wrapper around VPP agent client
 */
struct vac_t {
    void (*destroy)(vac_t *this);

    status_t (*put)(vac_t *this,
                    Vpp__ConfigData *data,
                    Dataconfigurator__UpdateResponse **rp);

    status_t (*del)(vac_t *this,
                    Vpp__ConfigData *data,
                    Dataconfigurator__DeleteResponse **rp);

    status_t (*dump_interfaces)(vac_t *this,
                                Dataconfigurator__DumpResponse **rp);

    status_t (*dump_routes)(vac_t *this,
                            Dataconfigurator__DumpResponse **rp);

    status_t (*dump_ipsec_tunnels)(vac_t *this,
                                   Dataconfigurator__DumpResponse **rp);

    status_t (*dump_punts)(vac_t *this,
                           Dataconfigurator__DumpResponse **rp);

    status_t (*register_events)(vac_t *this,
            Dataconfigurator__NotificationRequest *rq,
            grpc_c_client_callback_t *cb, void *tag);
};

extern vac_t *vac;

vac_t *vac_create(char *name);

#endif /* KERNEL_VPP_GRPC_H_ */
