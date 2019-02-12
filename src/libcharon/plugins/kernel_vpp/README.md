# VPP plugin #

## Overview ##
The kernel-vpp plugin is an interface to the IPsec and networking backend for
[**VPP**](https://wiki.fd.io/view/VPPhttps://wiki.fd.io/view/VPP) platform
using [**vpp-agent**](https://github.com/ligato/vpp-agent). It provides
address and routing lookup functionality and installs routes for IPsec traffic.
It installs and maintains IPSec tunnel interfaces in VPP, see
[**VPP IPsec**](https://wiki.fd.io/view/VPP/IPSec_and_IKEv2#IPSec).
The socket-vpp plugin is a replacement for socket-default for the VPP.
It provides an IPv4/IPv6 IKE socket backend based on the VPP UDP punt socket.
The plugin initializes VPP UDP IPv4 and IPv6 punt socket for IKE ports 500.

Both kernel-vpp and socket-vpp uses `gRPC` for communication with vpp-agent
by default. Optionally they can instead use `redis` if configured in `kernel-vpp.conf`.

## How to build strongSwan for VPP ##
In order to build strongSwan with VPP plugin enabled easiest way is to run:

    ./build_all.sh

which first pulls and builds all dependenies and installs them, then proceeds
with building strongSwan and its plugins. Afterwards install strongSwan with

    make install

Currently Ubuntu 18.04 is supported.

## Configuring VPP strongSwan ##
VPP specific configuration can be found in `/etc/strongswan.d/charon/kernel-vpp.conf`
or `socket-vpp.conf` respectively.

## Running test environment ##
Docker is required to run test environment:

    apt-get install docker.io

After installing docker and strongSwang run test env like following

    VPP_AGENT_VERSION=latest ./scripts/dev-env.sh start

where `VPP_AGENT_VERSION` defines docker image tag to run. Optionally you may
specify `VPP_CHANNEL=redis` which uses `redis` instead of `gRPC`.

This runs a topology shown below and establishes IPSec tunnel between initiator
and responder which enables communiaction between their private subnets
`10.10.10.0/24` and `10.10.20.0/24`.

    +-------------------------------------------------+
    |  +---------------+                              |
    |  | docker image  |                              |
    |  | (responder)   |                              |
    |  | +---------+   |172.16.0.2                    |
    |  | |vpp-agent+---+----+                         |
    |  | |& vpp    |   |    |        +--------------+ |
    |  | ++--------+   |    |        | docker image | |
    |  |  |            |    |        | (initiator)  | |
    |  |  +10.10.10.1  |    +--------+              | |
    |  |   (private IP)|   172.16.0.1|  strongSwan  | |
    |  +--+----------+-+             |  10.10.20.1  | |
    |     |          |               | (private IP) | |
    | punt sockets   |               +--------------+ |
    | (read + write) + gRPC                           |
    |     |          |                                |
    |     |          |                                |
    | +---+----------++                               |
    | |  strongSwan   |                               |
    | | (from project)|                               |
    | |               |                               |
    | +---------------+                               |
    |host                                             |
    +-------------------------------------------------+

To stop the test topology run:

    ./scripts/dev-env.sh stop


## Packaging ##
To build a debian package strongSwan must be built and installed. Then do

    cd deb
    make prepare
    make build

This creates a .deb file in `<project>/deb` directory.
