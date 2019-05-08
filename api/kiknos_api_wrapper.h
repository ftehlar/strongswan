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

/* Wrapper around C++ proto bindings */

#ifndef __KIKNOS__C_API_H__
#define __KIKNOS__C_API_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KIKNOS_NONE_CRYPTO = 0,
    KIKNOS_AES_CBC_128 = 1,
    KIKNOS_AES_CBC_192 = 2,
    KIKNOS_AES_CBC_256 = 3,
    KIKNOS_AES_CTR_128 = 4,
    KIKNOS_AES_CTR_192 = 5,
    KIKNOS_AES_CTR_256 = 6,
    KIKNOS_AES_GCM_128 = 7,
    KIKNOS_AES_GCM_192 = 8,
    KIKNOS_AES_GCM_256 = 9,
    KIKNOS_DES_CBC = 10,
    KIKNOS_DES3_CBC = 11,
}KiknosCryptoAlg;

typedef enum {
    KIKNOS_NONE_INTEG = 0,
    KIKNOS_MD5_96 = 1,
    KIKNOS_SHA1_96 = 2,
    KIKNOS_SHA_256_96 = 3,
    KIKNOS_SHA_256_128 = 4,
    KIKNOS_SHA_384_192 = 5,
    KIKNOS_SHA_512_256 = 6,
}KiknosIntegAlg;

typedef enum {
    KIKNOS_RC_OK = 0,
    KIKNOS_RC_FAIL = 1,
} kiknos_rc_t;

typedef struct {
    char *dst_net;
    char *next_hop;
    char *outgoing_interface;
    uint32_t preference;
} kiknos_route_t;

typedef struct {
    uint32_t port;
    char *socket_path;
} kiknos_punt_t;

typedef struct {
    char *name;
    int esn;
    char *local_ip;
    char *remote_ip;
    uint32_t local_spi;
    uint32_t remote_spi;

    KiknosCryptoAlg crypto_alg;
    char *local_crypto_key;
    char *remote_crypto_key;

    KiknosIntegAlg integ_alg;
    char *local_integ_key;
    char *remote_integ_key;

    int enable_udp_encap;
    char *interface_with_ip;
} kiknos_ipsec_tunnel_t;

kiknos_rc_t kiknos_client_create (const char *host);

kiknos_rc_t kiknos_add_route(kiknos_route_t *route);
kiknos_rc_t kiknos_del_route(kiknos_route_t *route);

kiknos_rc_t kiknos_add_tunnel(kiknos_ipsec_tunnel_t *tun);
kiknos_rc_t kiknos_del_tunnel(kiknos_ipsec_tunnel_t *tun);

kiknos_rc_t kiknos_add_punt_sockets (kiknos_punt_t *punts, int count);
char * kiknos_get_agent_punt_socket(void);

char * kiknos_get_if_name_by_ip(char *ip);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __KIKNOS__C_API_H__ */
