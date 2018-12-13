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

#include <daemon.h>
#include <utils/debug.h>
#include <collections/hashtable.h>
#include <threading/mutex.h>

#include "vpp/model/rpc/rpc.grpc-c.h"
#include "kernel_vpp_ipsec.h"
#include "kernel_vpp_grpc.h"

#include <errno.h>
#include <string.h>

#define IPV4_SZ 4
#define PRIO_BASE 384
#define MAX_UINT32_LEN 10
#define IF_NAME_PREFIX "tun-"

typedef struct private_kernel_vpp_ipsec_t private_kernel_vpp_ipsec_t;

typedef enum {
  ROUTE_DEL,
  ROUTE_ADD
} routes_op_e;

/**
 * Private variables of kernel_vpp_ipsec class.
 */
struct private_kernel_vpp_ipsec_t {

    /**
     * Public interface
     */
    kernel_vpp_ipsec_t public;

    /**
     * Mutex to lock access to installed policies
     */
    mutex_t *mutex;

    /**
     * Hash table containing cache of partially filed tunnels
     */
    hashtable_t *cache;

    /**
     * Linked list of created ipsec tunnels
     */
    hashtable_t *tunnels;

    /**
     * Next SPI to allocate
     */
    refcount_t nextspi;

    /**
     * Mix value to distribute SPI allocation randomly
     */
    uint32_t mixspi;

    /**
     * Whether to install routes along policies
     */
    bool manage_routes;

    /**
     * Next tunnel index
     */
    uint32_t next_index;
};

/**
 * VPP Tunnel Interface
 */
typedef struct {

    /**
     * Name of the ipsec tunnel interface
     */
    char *if_name;

    /**
     * Name of the interface we borrowed
     * IP from
     */
    char *un_if_name;

    /**
     * Source(Local) SPI
     */
    uint32_t src_spi;

    /**
     * Destination(Remote) SPI
     */
    uint32_t dst_spi;

    /**
     * Source(Local) IP
     */
    char *src_addr;

    /**
     * Destination(Remote) IP
     */
    char *dst_addr;

    /**
     * VPP Encryption algorithm
     */
    uint16_t enc_alg;

    /**
     * VPP Integrity algorithm
     */
    uint16_t int_alg;

    /**
     * Source(Local) Encryption key in hex
     */
    char *src_enc_key;

    /**
     * Destination(Remote) Encryption key in hex
     */
    char *dst_enc_key;

    /**
     * Source(Local) Integrity key in hex
     */
    char *src_int_key;

    /**
     * Destination(Remote) Integrity key in hex
     */
    char *dst_int_key;

} tunnel_t;

/**
 * Convert chunk to ipv4 address
 */
char *chunk_to_ipv4(chunk_t address)
{
    struct in_addr addr;
    char *ipv4;

    if (address.len != IPV4_SZ)
    {
        DBG2(DBG_KNL, "kernel_vpp: ip address unsupported size");
        return NULL;
    }

    ipv4 = calloc(INET_ADDRSTRLEN, sizeof(char));
    if (ipv4 == NULL)
    {
        return NULL;
    }

    memcpy(&(addr.s_addr), address.ptr, IPV4_SZ); 
    if (inet_ntop(AF_INET, &addr, ipv4, INET_ADDRSTRLEN * sizeof(char)) == NULL)
    {
        free(ipv4);
        DBG2(DBG_KNL, "kernel_vpp: inet_ntop error: %s", strerror(errno));
        return NULL;
    }
    return ipv4;
}

static void free_tunnel(tunnel_t *tunnel)
{
    if (tunnel->if_name)
        free(tunnel->if_name);
    if (tunnel->un_if_name)
        free(tunnel->un_if_name);
    if (tunnel->src_addr)
        free(tunnel->src_addr);
    if (tunnel->dst_addr)
        free(tunnel->dst_addr);
    if (tunnel->src_enc_key)
        free(tunnel->src_enc_key);
    if (tunnel->src_int_key)
        free(tunnel->src_int_key);
    if (tunnel->dst_enc_key)
        free(tunnel->dst_enc_key);
    if (tunnel->dst_int_key)
        free(tunnel->dst_int_key);
    free(tunnel);
}

/**
 * Prints out tunnel, used for debug purposes
 */
static void dump_tunnel(tunnel_t *tp)
{
    const char *q = "NULL";
    DBG1(DBG_KNL, "if_name: %s, un_if_name: %s, src_spi: %u, dst_spi: %u, " \
                  "src_addr: %s, dst_addr: %s, enc_alg: %d, int_alg: %d, " \
                  "src_enc_key: %s, dst_enc_key: %s, " \
                  "src_int_key: %s, dst_int_key: %s",
                  tp->if_name ? tp->if_name : q, 
                  tp->un_if_name ? tp->un_if_name : q,
                  tp->src_spi, tp->dst_spi,
                  tp->src_addr ? tp->src_addr : q,
                  tp->dst_addr ? tp->dst_addr : q,
                  tp->enc_alg, tp->int_alg,
                  tp->src_enc_key ? tp->src_enc_key : q,
                  tp->dst_enc_key ? tp->dst_enc_key : q,
                  tp->src_int_key ? tp->src_int_key : q,
                  tp->dst_int_key ? tp->dst_int_key : q);
}

/**
 * Hash function for IPsec Tunnel Interface hash table
 */
static u_int tunnel_hash(tunnel_t *tunnel)
{
    return chunk_hash(chunk_from_thing(tunnel->dst_spi));
}

/**
 * Equals function for IPSec Tunnel Interface hash table
 */
static bool tunnel_equals(tunnel_t *one, tunnel_t *two)
{
    return one->dst_spi == two->dst_spi &&
           one->dst_addr && two->dst_addr &&
           (strcmp(one->dst_addr, two->dst_addr) == 0);
}

/**
 * Converts strongswan encryption algorithm numbering to vpp numbering
 */
/* ENCR_3DES (4) NOT supported in proto definition */
static status_t convert_enc_alg(uint16_t alg, chunk_t key, uint16_t *vpp_alg)
{
    if (ENCR_NULL == alg)
    {
        *vpp_alg = IPSEC__CRYPTO_ALGORITHM__NONE_CRYPTO;
    }
    else if (ENCR_AES_CBC == alg)
    {
        switch (key.len * 8)
        {
            case 128:
                *vpp_alg = IPSEC__CRYPTO_ALGORITHM__AES_CBC_128;
                break;
            case 192:
                *vpp_alg = IPSEC__CRYPTO_ALGORITHM__AES_CBC_192;
                break;
            case 256:
                *vpp_alg = IPSEC__CRYPTO_ALGORITHM__AES_CBC_256;
                break;
            default:
                return FAILED;
        }
    }
    else
    {
        return FAILED;
    }
    return SUCCESS;
}

/**
 * Converts strongswan integrity algorithm numbering to vpp numbering
 */
static status_t convert_int_alg(uint16_t alg, uint16_t *vpp_alg)
{
    switch (alg)
    {
        case AUTH_UNDEFINED:
            *vpp_alg = IPSEC__INTEG_ALGORITHM__NONE_INTEG;
            break;
        case AUTH_HMAC_MD5_96:
            *vpp_alg = IPSEC__INTEG_ALGORITHM__MD5_96;
            break;
        case AUTH_HMAC_SHA1_96:
            *vpp_alg = IPSEC__INTEG_ALGORITHM__SHA1_96;
            break;
        case AUTH_HMAC_SHA2_256_128:
            *vpp_alg = IPSEC__INTEG_ALGORITHM__SHA_256_128;
            break;
        case AUTH_HMAC_SHA2_384_192:
            *vpp_alg = IPSEC__INTEG_ALGORITHM__SHA_384_192;
            break;
        case AUTH_HMAC_SHA2_512_256:
            *vpp_alg = IPSEC__INTEG_ALGORITHM__SHA_512_256;
            break;
        default:
            return FAILED;
    }
    return SUCCESS;
}

/**
 * Delete tunnel interface over grpc
 */
static status_t delete_tunnel(tunnel_t *tp)
{
    Ipsec__TunnelInterfaces__Tunnel tunnel = IPSEC__TUNNEL_INTERFACES__TUNNEL__INIT;
    Ipsec__TunnelInterfaces__Tunnel *tunnels[1];

    Rpc__DataRequest req = RPC__DATA_REQUEST__INIT;
    Rpc__DelResponse *rsp = NULL;

    req.tunnels = tunnels;
    req.tunnels[0] = &tunnel;
    req.n_tunnels = 1;

    status_t rc;

    tunnel.name = tp->if_name;

    rc = vac->del(vac, &req, &rsp); 
    if (rc == FAILED)
    {
        DBG1(DBG_KNL, "kernel_vpp: error communicating with grpc");
        return FAILED;
    }

    rpc__del_response__free_unpacked(rsp, 0);
    return SUCCESS;
}

/**
 * Add or remove a route
 */
static status_t vpp_add_del_route(private_kernel_vpp_ipsec_t *this,
                                  kernel_ipsec_policy_id_t *id,
                                  kernel_ipsec_manage_policy_t *data,
                                  routes_op_e op)
{
    tunnel_t *tunnel;
    host_t *dst_net;
    uint8_t pfx_len;
    status_t rc = SUCCESS;
    

    if ((data->type != POLICY_IPSEC) || !data->sa ||
        (data->sa->mode != MODE_TUNNEL))
    {
        DBG1(DBG_KNL, "kernel_vpp: unsupported SA received");
        return NOT_SUPPORTED;
    }

    // we ignore POLICY_IN
    // because we use this call ony for setting up routes
    if (id->dir != POLICY_OUT)
    {
        return SUCCESS;
    }

    tunnel_t _tun = {
            .dst_spi = ntohl(data->sa->esp.spi),
            .dst_addr = chunk_to_ipv4(data->dst->get_address(data->dst))
    };

    if (op == ROUTE_ADD)
    {
        this->mutex->lock(this->mutex);
        tunnel = this->tunnels->get(this->tunnels, &_tun);
        this->mutex->unlock(this->mutex);
    }
    else {
        this->mutex->lock(this->mutex);
        tunnel = this->tunnels->remove(this->tunnels, &_tun);
        this->mutex->unlock(this->mutex);
    }

    free(_tun.dst_addr);

    if (!tunnel)
    {
        DBG1(DBG_KNL, "kernel_vpp: error %s routes, "
                      "tunnel not found", op == ROUTE_ADD ?
                      "adding" : "removing");
        return FAILED;
    }

    dump_tunnel(tunnel);

    id->dst_ts->to_subnet(id->dst_ts, &dst_net, &pfx_len);

    if (op == ROUTE_ADD)
    {
        rc = charon->kernel->add_route(charon->kernel, dst_net->get_address(
                                       dst_net), pfx_len, data->dst, NULL,
                                       tunnel->if_name);
    }
    else
    {
        rc = charon->kernel->del_route(charon->kernel, dst_net->get_address(
                                       dst_net), pfx_len, data->dst, NULL,
                                       tunnel->if_name);
        if (delete_tunnel(tunnel) != SUCCESS)
        {
            DBG1(DBG_KNL, "kernel_vpp: error deleting tunnel");
            rc = FAILED;
        }
        else
        {
            DBG1(DBG_KNL, "kernel_vpp: success deleting tunnel");
        }
        free_tunnel(tunnel);
    }

    DBG1(DBG_KNL, "kernel_vpp: (%s) %s route %H/%d via tunnel interface %s",
         rc == SUCCESS ? "success" : "failure",
         op == ROUTE_ADD ? "add" : "del",
         dst_net, pfx_len, tunnel->if_name);

    return rc;
}

/**
 * Create tunnel interface over grpc
 */
static status_t create_tunnel(tunnel_t *tp)
{
    Ipsec__TunnelInterfaces__Tunnel tunnel = IPSEC__TUNNEL_INTERFACES__TUNNEL__INIT;
    Ipsec__TunnelInterfaces__Tunnel *tunnels[1];

    Rpc__DataRequest req = RPC__DATA_REQUEST__INIT;
    Rpc__PutResponse *rsp = NULL;

    req.tunnels = tunnels;
    req.tunnels[0] = &tunnel;
    req.n_tunnels = 1;

    status_t rc;

    tunnel.has_enabled = TRUE;
    tunnel.has_local_spi = TRUE;
    tunnel.has_remote_spi = TRUE;
    tunnel.has_integ_alg = TRUE;
    tunnel.has_crypto_alg = TRUE;

    tunnel.name = tp->if_name;

    tunnel.enabled = TRUE;
    tunnel.unnumbered_name = tp->un_if_name;

    tunnel.integ_alg = tp->int_alg;
    tunnel.crypto_alg = tp->enc_alg;
    
    tunnel.local_ip = tp->src_addr;
    tunnel.local_spi = tp->src_spi;
    tunnel.local_integ_key = tp->src_int_key;
    tunnel.local_crypto_key = tp->src_enc_key;

    tunnel.remote_ip = tp->dst_addr;
    tunnel.remote_spi = tp->dst_spi;
    tunnel.remote_integ_key = tp->dst_int_key;
    tunnel.remote_crypto_key = tp->dst_enc_key;
    
    rc = vac->put(vac, &req, &rsp); 
    if (rc == FAILED)
    {
        DBG1(DBG_KNL, "kernel_vpp: error communicating with grpc");
        return FAILED;
    }

    rpc__put_response__free_unpacked(rsp, 0);
    return SUCCESS;
}

METHOD(kernel_ipsec_t, add_sa, status_t,
    private_kernel_vpp_ipsec_t *this, kernel_ipsec_sa_id_t *id,
    kernel_ipsec_add_sa_t *data)
{
    uint16_t vpp_enc_alg;
    uint16_t vpp_int_alg;

    chunk_t enc_key;
    chunk_t int_key;

    char *if_name, *un_if_name;
    tunnel_t *tunnel;
    status_t rc;
    size_t len;

    if (data->mode != MODE_TUNNEL)
    {
        return NOT_SUPPORTED;
    }

    // inbound SA comes first
    if (data->inbound)
    {
        rc = convert_enc_alg(data->enc_alg, data->enc_key, &vpp_enc_alg);
        if (rc != SUCCESS)
        {
            DBG1(DBG_KNL, "kernel_vpp: algorithm %N not supported by VPP!",
                 encryption_algorithm_names, data->enc_alg);
            return NOT_SUPPORTED;
        }

        rc = convert_int_alg(data->int_alg, &vpp_int_alg);
        if (rc != SUCCESS)
        {
            DBG1(DBG_KNL, "kernel_vpp: algorithm %N not supported by VPP!",
                 integrity_algorithm_names, data->int_alg);
            return NOT_SUPPORTED;
        }

        if (!charon->kernel->get_interface(charon->kernel, id->dst, &un_if_name))
        {
            DBG1(DBG_KNL, "kernel_vpp: unable to get interface %H", id->dst);
            return FAILED;
        }

        enc_key = chunk_to_hex(data->enc_key, NULL, 0);
        int_key = chunk_to_hex(data->int_key, NULL, 0);

        len = strlen(IF_NAME_PREFIX) + MAX_UINT32_LEN + 1;
        if_name = malloc(sizeof(char) * len);

        this->mutex->lock(this->mutex);
        snprintf(if_name, len, "%s%d", IF_NAME_PREFIX, this->next_index++);
        this->mutex->unlock(this->mutex);

        INIT(tunnel,
               .if_name = if_name,
               .un_if_name = un_if_name,
               .src_spi = ntohl(id->spi),
               .src_addr = chunk_to_ipv4(id->dst->get_address(id->dst)),
               .dst_addr = chunk_to_ipv4(id->src->get_address(id->src)),
               .enc_alg = vpp_enc_alg,
               .int_alg = vpp_int_alg,
               .src_enc_key = enc_key.ptr,
               .src_int_key = int_key.ptr
            );

        if (!tunnel->src_addr || !tunnel->dst_addr)
        {
            free_tunnel(tunnel);
            DBG1(DBG_KNL, "kernel_vpp: error converting chunk to ipv4 %s"
                          " address", !tunnel->src_addr ? "src" : "dst");
            return FAILED;
        }

        this->mutex->lock(this->mutex);
        this->cache->put(this->cache, (void *)(uintptr_t)data->reqid, tunnel);
        this->mutex->unlock(this->mutex);

        DBG1(DBG_KNL, "kernel_vpp: success caching tunnel, received inboud SA");
        dump_tunnel(tunnel);
    }
    else
    {
        this->mutex->lock(this->mutex);
        tunnel = this->cache->remove(this->cache,
                                     (void *)(uintptr_t)data->reqid);
        this->mutex->unlock(this->mutex);

        if (!tunnel)
        {
            DBG1(DBG_KNL, "kernel_vpp: error adding tunnel, "
                          "missing inbound SA");
            return NOT_FOUND;
        }

        enc_key = chunk_to_hex(data->enc_key, NULL, 0);
        int_key = chunk_to_hex(data->int_key, NULL, 0);

        tunnel->dst_enc_key = enc_key.ptr;
        tunnel->dst_int_key = int_key.ptr;

        tunnel->dst_spi = ntohl(id->spi);

        if (create_tunnel(tunnel) == FAILED)
        {
            free_tunnel(tunnel);
            DBG1(DBG_KNL, "kernel_vpp: error creating tunnel");
            return FAILED;
        }

        DBG1(DBG_KNL, "kernel_vpp: success creating tunnel, "
                      "received outbound SA");
        dump_tunnel(tunnel);

        this->mutex->lock(this->mutex);
        this->tunnels->put(this->tunnels, tunnel, tunnel);
        this->mutex->unlock(this->mutex);
    }
    return SUCCESS;
}

METHOD(kernel_ipsec_t, del_sa, status_t,
    private_kernel_vpp_ipsec_t *this, kernel_ipsec_sa_id_t *id,
    kernel_ipsec_del_sa_t *data)
{
    return SUCCESS;
}

METHOD(kernel_ipsec_t, add_policy, status_t,
    private_kernel_vpp_ipsec_t *this, kernel_ipsec_policy_id_t *id,
    kernel_ipsec_manage_policy_t *data)
{
    if (this->manage_routes)
    {
        return vpp_add_del_route(this, id, data, ROUTE_ADD);
    }
    return SUCCESS;
}

METHOD(kernel_ipsec_t, del_policy, status_t,
    private_kernel_vpp_ipsec_t *this, kernel_ipsec_policy_id_t *id,
    kernel_ipsec_manage_policy_t *data)
{
    if (this->manage_routes)
    {
        return vpp_add_del_route(this, id, data, ROUTE_DEL);
    }
    return SUCCESS;
}

METHOD(kernel_ipsec_t, query_sa, status_t,
    private_kernel_vpp_ipsec_t *this, kernel_ipsec_sa_id_t *id,
    kernel_ipsec_query_sa_t *data, uint64_t *bytes, uint64_t *packets,
    time_t *time)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_ipsec_t, get_features, kernel_feature_t,
    private_kernel_vpp_ipsec_t *this)
{
    return KERNEL_ESP_V3_TFC;
}

/**
 * Map an integer x with a one-to-one function using quadratic residues
 */
static u_int permute(u_int x, u_int p)
{
    u_int qr;

    x = x % p;
    qr = ((uint64_t)x * x) % p;
    if (x <= p / 2)
    {
        return qr;
    }
    return p - qr;
}

/**
 * Initialize seeds for SPI generation
 */
static bool init_spi(private_kernel_vpp_ipsec_t *this)
{
    bool ok = TRUE;
    rng_t *rng;

    rng = lib->crypto->create_rng(lib->crypto, RNG_STRONG);
    if (!rng)
    {
        return FALSE;
    }
    ok = rng->get_bytes(rng, sizeof(this->nextspi), (uint8_t*)&this->nextspi);
    if (ok)
    {
        ok = rng->get_bytes(rng, sizeof(this->mixspi), (uint8_t*)&this->mixspi);
    }
    rng->destroy(rng);
    return ok;
}

METHOD(kernel_ipsec_t, get_spi, status_t,
    private_kernel_vpp_ipsec_t *this, host_t *src, host_t *dst,
    uint8_t protocol, uint32_t *spi)
{
    static const u_int p = 268435399, offset = 0xc0000000;

    *spi = htonl(offset + permute(ref_get(&this->nextspi) ^ this->mixspi, p));
    return SUCCESS;
}

static void free_cache(private_kernel_vpp_ipsec_t *this)
{
    void *key;
    tunnel_t *val;
    enumerator_t *enumerator;

    this->mutex->lock(this->mutex);
    enumerator = this->cache->create_enumerator(this->cache);
    while (enumerator->enumerate(enumerator, &key, &val))
    {
        free_tunnel(val);
    }
    enumerator->destroy(enumerator);
    this->mutex->unlock(this->mutex);
}

static void free_tunnels(private_kernel_vpp_ipsec_t *this)
{
    void *key;
    tunnel_t *val;
    enumerator_t *enumerator;

    this->mutex->lock(this->mutex);
    enumerator = this->tunnels->create_enumerator(this->tunnels);
    while (enumerator->enumerate(enumerator, &key, &val))
    {
        free_tunnel(val);
    }
    enumerator->destroy(enumerator);
    this->mutex->unlock(this->mutex);
}

METHOD(kernel_ipsec_t, destroy, void,
    private_kernel_vpp_ipsec_t *this)
{
    free_cache(this);
    free_tunnels(this);

    this->mutex->destroy(this->mutex);
    this->cache->destroy(this->cache);
    this->tunnels->destroy(this->tunnels);
    free(this);
}

METHOD(kernel_ipsec_t, get_cpi, status_t,
    private_kernel_vpp_ipsec_t *this, host_t *src, host_t *dst,
    uint16_t *cpi)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_ipsec_t, update_sa, status_t,
    private_kernel_vpp_ipsec_t *this, kernel_ipsec_sa_id_t *id,
    kernel_ipsec_update_sa_t *data)
{
    DBG1(DBG_KNL, "kernel_vpp: update sa requested");
    return NOT_SUPPORTED;
}

METHOD(kernel_ipsec_t, flush_sas, status_t,
    private_kernel_vpp_ipsec_t *this)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_ipsec_t, query_policy, status_t,
    private_kernel_vpp_ipsec_t *this, kernel_ipsec_policy_id_t *id,
    kernel_ipsec_query_policy_t *data, time_t *use_time)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_ipsec_t, flush_policies, status_t,
    private_kernel_vpp_ipsec_t *this)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_ipsec_t, bypass_socket, bool,
    private_kernel_vpp_ipsec_t *this, int fd, int family)
{
    return NOT_SUPPORTED;
}

METHOD(kernel_ipsec_t, enable_udp_decap, bool,
    private_kernel_vpp_ipsec_t *this, int fd, int family, u_int16_t port)
{
    return NOT_SUPPORTED;
}

kernel_vpp_ipsec_t *kernel_vpp_ipsec_create()
{
    private_kernel_vpp_ipsec_t *this;

    INIT(this,
        .public = {
            .interface = {
                .get_features = _get_features,
                .get_spi = _get_spi,
                .get_cpi = _get_cpi,
                .add_sa  = _add_sa,
                .update_sa = _update_sa,
                .query_sa = _query_sa,
                .del_sa = _del_sa,
                .flush_sas = _flush_sas,
                .add_policy = _add_policy,
                .query_policy = _query_policy,
                .del_policy = _del_policy,
                .flush_policies = _flush_policies,
                .bypass_socket = _bypass_socket,
                .enable_udp_decap = _enable_udp_decap,
                .destroy = _destroy,
            },
        },
        .mutex = mutex_create(MUTEX_TYPE_DEFAULT),
        .cache = hashtable_create(hashtable_hash_ptr, hashtable_equals_ptr, 4),
        .tunnels = hashtable_create((void *)tunnel_hash,
                                    (void *)tunnel_equals, 1),
        .manage_routes = lib->settings->get_bool(lib->settings,
                            "%s.install_routes", TRUE, lib->ns),
    );

    if (!init_spi(this))
    {
        destroy(this);
        return NULL;
    }

    return &this->public;
}

