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
#include "socket_vpp_socket.h"

#include <sys/un.h>
#include <sys/socket.h>
#include <errno.h>
#include <unistd.h>
#include <ipsec.h>
#include <daemon.h>
#include <threading/thread.h>
#include <kernel_vpp_grpc.h>
#include <ip_packet.h>

#include "api/kiknos_api_wrapper.h"

#define SOCK_NAME_PORT "sock_port_path"
#define SOCK_NAME_NATT "sock_natt_path"

#define SOCK_PATH_PORT "/etc/vpp/" SOCK_NAME_PORT
#define SOCK_PATH_NATT "/etc/vpp/" SOCK_NAME_NATT

typedef struct private_socket_vpp_socket_t private_socket_vpp_socket_t;
typedef struct vpp_packetdesc_t vpp_packetdesc_t;
typedef struct ether_header_t ether_header_t;

/**
 * Private data of an socket_t object
 */
struct private_socket_vpp_socket_t {

    /**
     * public functions
     */
    socket_vpp_socket_t public;

    /**
     * maximum packet size to receive
     */
    int max_packet;

    /**
     * Write socket
     */
    struct sockaddr_un write_addr;

    /**
     * Socket
     */
    int sock_port;

    /**
     * Socket for NAT-T
     */
    int sock_natt;

    /**
     * Configured IKEv2 port
     */
    uint16_t port;

    /**
     * Configured NAT-T port
     */
    uint16_t natt;

    /**
     * Port address
     */
    struct sockaddr_un addr_port;

    /**
     * NAT-T address. Used only when default ports are configured
     */
    struct sockaddr_un addr_natt;

    /**
     * VPP Agent client
     */
    vac_t *vac;

    /**
     * Helper varibale used for round-robin algorithm when receiving
     * from multiple sockets
     */
    int rr_index;

    /**
     * Socket registration retry thread
     */
    thread_t *reg_retry;

    bool ports_registered;
    char *sock_port_path;
    char *sock_natt_path;
};

/**
 * VPP punt socket action
 */
enum {
    PUNT_L2 = 0,
    PUNT_IP4_ROUTED,
    PUNT_IP6_ROUTED,
};

/**
 * VPP punt socket packet descriptor header
 */
struct vpp_packetdesc_t {
    /** RX or TX interface */
    u_int sw_if_index;
    /** action */
    int action;
} __attribute__((packed));

/**
 * Ethernet header
 */
struct ether_header_t {
    /** src MAC */
    uint8_t src[6];
    /** dst MAC */
    uint8_t dst[6];
    /** EtherType */
    uint16_t type;
} __attribute__((packed));

METHOD(socket_t, receiver, status_t,
    private_socket_vpp_socket_t *this, packet_t **out)
{
    int rr, ri, i, bytes_read = 0;
    host_t *src = NULL, *dst = NULL;
    char buf[this->max_packet];
    packet_t *pkt;
    bool old;

    struct pollfd pfd[] = {
            {.fd = this->sock_port, .events = POLLIN },
            {.fd = this->sock_natt, .events = POLLIN }
    };

    DBG2(DBG_NET, "socket_vpp: waiting for packets");
    old = thread_cancelability(TRUE);
    if (poll(pfd, countof(pfd), -1) <= 0)
    {
        thread_cancelability(old);
        DBG1(DBG_NET, "socket_vpp: error polling sockets");
        return FAILED;
    }
    thread_cancelability(old);

    ri = -1;
    rr = ++this->rr_index;
    this->rr_index = rr = (rr % countof(pfd)) != rr ? 0 : rr;

    if (!(pfd[rr].revents & POLLIN))
    {
        // do 0 -> rr and rr -> count
        for (i = 0; i < countof(pfd); i++)
        {
            if (i == rr)
                continue;
            if (pfd[i].revents & POLLIN)
            {
                this->rr_index = ri = i;
                break;
            }
        }
    }
    else
    {
        ri = rr;
    }

    if (ri >= 0)
    {
        struct msghdr msg;
        memset(&msg, 0, sizeof(msg));
        struct iovec iov[3];
        vpp_packetdesc_t packetdesc;
        ether_header_t eh;
        ip_packet_t *ip_packet;
        chunk_t raw, data;

        iov[0].iov_base = &packetdesc;
        iov[0].iov_len = sizeof(packetdesc);
        iov[1].iov_base = &eh;
        iov[1].iov_len = sizeof(eh);
        iov[2].iov_base = buf;
        iov[2].iov_len = this->max_packet;
        msg.msg_iov = iov;
        msg.msg_iovlen = 3;

        bytes_read = recvmsg(pfd[ri].fd, &msg, 0);
        if (bytes_read < 0)
        {
            DBG1(DBG_NET, "socket_vpp: error reading data '%s'",
                 strerror(errno));
            return FAILED;
        }
        DBG3(DBG_NET, "socket_vpp: received packet '%b'", buf, bytes_read);

        raw = chunk_create(buf, bytes_read);
        ip_packet = ip_packet_create(chunk_clone(raw));
        if (!ip_packet)
        {
            DBG1(DBG_NET, "socket_vpp: invalid IP packet read from vpp socket");
            return FAILED;
        }
        src = ip_packet->get_source(ip_packet);
        dst = ip_packet->get_destination(ip_packet);
        pkt = packet_create();
        pkt->set_source(pkt, src->clone(src));
        pkt->set_destination(pkt, src->clone(dst));

        data = ip_packet->get_payload(ip_packet);

        /* remove UDP header */
        data = chunk_skip(data, 8);
        pkt->set_data(pkt, chunk_clone(data));
        ip_packet->destroy(ip_packet);

        DBG2(DBG_NET, "socket_vpp: received packet from %#H to %#H", src, dst);
    }
    else
    {
        return FAILED;
    }

    *out = pkt;
    return SUCCESS;
}

METHOD(socket_t, sender, status_t,
       private_socket_vpp_socket_t *this, packet_t *packet)
{
    struct msghdr msg;
    struct iovec iov[2];
    vpp_packetdesc_t packetdesc;
    ssize_t bytes_sent;
    chunk_t data, raw;
    host_t *src, *dst;
    int family;
    ip_packet_t *ip_packet;

    src = packet->get_source(packet);
    dst = packet->get_destination(packet);
    data = packet->get_data(packet);
    if (!src->get_port(src))
    {
        src->set_port(src, this->port);
    }

    DBG2(DBG_NET, "sending vpp packet: from %#H to %#H", src, dst);

    family = dst->get_family(dst);

    packetdesc.sw_if_index = 0;
    if (family == AF_INET)
    {
        packetdesc.action = PUNT_IP4_ROUTED;
    }
    else
    {
        packetdesc.action = PUNT_IP6_ROUTED;
    }

    ip_packet = ip_packet_create_udp_from_data(src, dst, data);
    if (!ip_packet)
    {
        DBG1(DBG_NET, "create IP packet failed");
        return FAILED;
    }
    raw = ip_packet->get_encoding(ip_packet);
    memset(&msg, 0, sizeof(struct msghdr));
    iov[0].iov_base = &packetdesc;
    iov[0].iov_len = sizeof(packetdesc);
    iov[1].iov_base = raw.ptr;
    iov[1].iov_len = raw.len;
    msg.msg_iov = iov;
    msg.msg_iovlen = 2;
    msg.msg_name = &this->write_addr;
    msg.msg_namelen = sizeof(this->write_addr);
    msg.msg_flags = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;

    DBG1(DBG_NET, "socket_vpp: writing to %s", this->write_addr.sun_path);

    bytes_sent = sendmsg(this->sock_port, &msg, 0);
    ip_packet->destroy(ip_packet);
    if (bytes_sent < 0)
    {
        DBG1(DBG_NET, "socket_vpp: error writing: %s", strerror(errno));
        return FAILED;
    }

    return SUCCESS;
}

METHOD(socket_t, get_port, uint16_t,
    private_socket_vpp_socket_t *this, bool nat)
{
    return nat ? this->natt : this->port;
}

METHOD(socket_t, supported_families, socket_family_t,
    private_socket_vpp_socket_t *this)
{
    return SOCKET_FAMILY_BOTH;
}

METHOD(socket_t, destroy, void,
    private_socket_vpp_socket_t *this)
{
    close(this->sock_natt);
    unlink(this->addr_natt.sun_path);
    close(this->sock_port);
    unlink(this->addr_port.sun_path);
    free(this);
}

static status_t register_punt_sockets(vac_t *vac,
                                     uint16_t *ports,
                                     char **read_paths)
{
    kiknos_punt_t punts[2];
    memset(punts, 0,  2 * sizeof(punts[0]));
    int i;

    for (i = 0; i < 2; i++)
    {
        punts[i].port = ports[i];
        punts[i].socket_path = read_paths[i];
    }

    kiknos_rc_t rc = vac->add_punt_sockets(vac, punts, 2);
    if (KIKNOS_RC_OK != rc) {
        DBG1(DBG_LIB, "socket_vpp: register punt socket faield!");
        return FAILED;
    }
    return SUCCESS;
}

static status_t set_addr_name(struct sockaddr_un *saddr, char *path)
{
    size_t len = strlen(path);

    if (sizeof(saddr->sun_path) <= len)
    {
        DBG1(DBG_LIB, "socket_vpp: socket path is too long");
        return FAILED;
    }

    memset(saddr, 0, sizeof(*saddr));

    strncpy(saddr->sun_path, path, len);
    saddr->sun_family = AF_UNIX;

    return SUCCESS;
}

static status_t create_read_socket(struct sockaddr_un *saddr,
                                   char *path,
                                   int port,
                                   int *socket_out)
{
    int sock;
    DBG1(DBG_LIB, "socket_vpp: creating socket %s on port %u", path, port);

    if (set_addr_name(saddr, path) != SUCCESS)
        return FAILED;

    sock = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        DBG1(DBG_LIB, "socket_vpp: opening socket failed");
        return FAILED;
    }

    unlink(saddr->sun_path);
    if (bind(sock, (struct sockaddr *)saddr,
              sizeof(*saddr)) < 0)
    {
        close(sock);
        DBG1(DBG_LIB, "socket_vpp: binding socket failed");
        return FAILED;
    }

    *socket_out = sock;
    return SUCCESS;
}

static status_t get_vpp_socket_path(vac_t *vac, char **path)
{
    *path = vac->get_agent_punt_socket(vac);
    return *path ? SUCCESS : FAILED;
}

static status_t register_paths(private_socket_vpp_socket_t *this)
{
    status_t status;
    uint16_t ports[2] = {
        this->port,
        this->natt,
    };

    char *socket_paths[2] = {
        this->sock_port_path,
        this->sock_natt_path,
    };

    if (!this->ports_registered)
    {
        status = register_punt_sockets(this->vac, ports, socket_paths);
        if (status == SUCCESS)
        {
            this->ports_registered = TRUE;
        }
        else
        {
            DBG1(DBG_LIB, "socket_vpp: error registering punt sockets!");
            return FAILED;
        }
    }
    return SUCCESS;
}

static void *socket_vpp_register_thread(private_socket_vpp_socket_t *this)
{
    while (1)
    {
        if (SUCCESS == register_paths(this))
        {
            DBG2(DBG_LIB, "socket_vpp: socket register retry procedure complete");
            return NULL;
        }

        DBG2(DBG_LIB, "socket_vpp: socket registration failed, retrying");
        sleep(3);
    }
    return NULL;
}

/*
 * See header for description
 */
socket_vpp_socket_t *socket_vpp_socket_create()
{
    private_socket_vpp_socket_t *this;
    char *write_path = NULL;
    status_t rc;

    INIT(this,
        .public = {
            .socket = {
                .send = _sender,
                .receive = _receiver,
                .get_port = _get_port,
                .supported_families = _supported_families,
                .destroy = _destroy,
            },
        },
        .vac = lib->get(lib, "kernel-vpp-vac"),
        .max_packet = lib->settings->get_int(lib->settings, "%s.max_packet",
                                             PACKET_MAX_DEFAULT, lib->ns),
        .port = lib->settings->get_int(lib->settings, "%s.port",
                    CHARON_UDP_PORT, lib->ns),
        .natt = lib->settings->get_int(lib->settings, "%s.port_nat_t",
                    CHARON_NATT_PORT, lib->ns),
        .ports_registered = FALSE,
        .sock_port_path = lib->settings->get_str(lib->settings,
                            "%s.plugins.socket-vpp.sock_port_path",
                            SOCK_PATH_PORT, lib->ns),
        .sock_natt_path = lib->settings->get_str(lib->settings,
                            "%s.plugins.socket-vpp.sock_natt_path",
                            SOCK_PATH_NATT, lib->ns),
        .rr_index = 0
    );

    if (!this->vac)
    {
        DBG1(DBG_LIB, "socket_vpp: vac not available (missing plugin?)");
        return NULL;
    }

    if (this->port == 0 || this->natt == 0) {
        DBG1(DBG_LIB, "socket_vpp: random port allocation not supported!");
        return NULL;
    }

    if (this->port != CHARON_UDP_PORT ||
            this->natt != CHARON_NATT_PORT)
    {
        DBG1(DBG_LIB, "socket_vpp: custom UDP/NAT-T ports not supported!");
        return NULL;
    }

    rc = create_read_socket(&this->addr_port, this->sock_port_path, this->port,
                          &this->sock_port);
    if (SUCCESS != rc)
    {
        DBG1(DBG_LIB, "socket_vpp: error binding socket!");
        return NULL;
    }

    rc = create_read_socket(&this->addr_natt, this->sock_natt_path,
                          this->natt, &this->sock_natt);
    if (SUCCESS != rc)
    {
        DBG1(DBG_LIB, "socket_vpp: error binding nat-t socket!");
        close(this->sock_port);
        return NULL;
    }

    DBG2(DBG_LIB, "socket_vpp: starting socket register retry procedure");
    this->reg_retry = thread_create(
            (thread_main_t)socket_vpp_register_thread, this);

    /* keep waiting until registration is complete otherwise we cannot
     * get write path from vpp */
    this->reg_retry->join(this->reg_retry);

    rc = get_vpp_socket_path(this->vac, &write_path);
    if (SUCCESS != rc)
    {
        close(this->sock_natt);
        close(this->sock_port);
        return NULL;
    }

    rc = set_addr_name(&this->write_addr, write_path);
    if (SUCCESS != rc)
    {
        close(this->sock_natt);
        close(this->sock_port);
        free(write_path);
        return NULL;
    }
    DBG1(DBG_LIB, "socket_vpp: write path received from vpp: %s", write_path);
    DBG2(DBG_LIB, "socket_vpp: success initializing plugin");

    free(write_path);
    return &this->public;
}

