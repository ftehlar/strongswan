#!/usr/bin/env bash

# Copyright (c) 2018-2019 Cisco and/or its affiliates.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at:
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -euo pipefail

WS="`pwd`"
AGENT_ROOT=${WS}/third_party/vpp-agent/api
API_DEST=third_party/vpp_agent_c_api

protos=$(find ${AGENT_ROOT} -type f -name '*.proto')

clean_file()
{
    sed '/github.com\|option go_package\|gogoproto/d' "${1}" > "${1}.new"
    rm "${1}"
    mv "${1}.new" "${1}"
}

# get rid of unecessary imports and options
clean_protos()
{
    for proto in $protos; do
        echo "Fixing $proto"
        clean_file "${proto}"
    done
}

generate_protos()
{

    for proto in $protos; do
        echo " - $proto";
        protoc \
            -I ${AGENT_ROOT}/models \
            -I ${AGENT_ROOT} \
            --grpc-c_out=${AGENT_ROOT} \
            --grpc-c_out=${AGENT_ROOT}/vpp \
            --grpc-c_out=${AGENT_ROOT}/linux \
            --plugin=third_party/grpc-c/build/compiler/protoc-gen-grpc-c \
            "$proto";
    done

    cp -r ${AGENT_ROOT}/configurator ${API_DEST}
    mkdir ${API_DEST}/models || true
    cp -r ${AGENT_ROOT}/{vpp,linux} ${API_DEST}/models
}

# generated headers contain same definitions used for preventing multiple include
fix_headers()
{
    sed 's/PROTOBUF_C_l3_2eproto__INCLUDED/PROTO_L3_H/' \
        ${API_DEST}/models/vpp/l3/l3.grpc-c.h > ${API_DEST}/models/vpp/l3/l3.grpc-c.h.new
    sed 's/PROTOBUF_C_interface_2eproto__INCLUDED/PROTO_INTERFACES_H/' \
        ${API_DEST}/models/vpp/interfaces/interface.grpc-c.h > ${API_DEST}/models/vpp/interfaces/interface.grpc-c.h.new
    sed 's/PROTOBUF_C_arp_2eproto__INCLUDED/PROTO_ARP_H/' \
        ${API_DEST}/models/vpp/l3/arp.grpc-c.h > ${API_DEST}/models/vpp/l3/arp.grpc-c.h.new
    sed 's/PROTOBUF_C_route_2eproto__INCLUDED/PROTO_ROUTE_H/' \
        ${API_DEST}/models/vpp/l3/route.grpc-c.h > ${API_DEST}/models/vpp/l3/route.grpc-c.h.new

    rm ${API_DEST}/models/vpp/l3/l3.grpc-c.h
    rm ${API_DEST}/models/vpp/interfaces/interface.grpc-c.h
    rm ${API_DEST}/models/vpp/l3/arp.grpc-c.h
    rm ${API_DEST}/models/vpp/l3/route.grpc-c.h

    mv ${API_DEST}/models/vpp/interfaces/interface.grpc-c.h.new \
        ${API_DEST}/models/vpp/interfaces/interface.grpc-c.h
    mv ${API_DEST}/models/vpp/l3/l3.grpc-c.h.new \
        ${API_DEST}/models/vpp/l3/l3.grpc-c.h
    mv ${API_DEST}/models/vpp/l3/arp.grpc-c.h.new \
        ${API_DEST}/models/vpp/l3/arp.grpc-c.h
    mv ${API_DEST}/models/vpp/l3/route.grpc-c.h.new \
        ${API_DEST}/models/vpp/l3/route.grpc-c.h
}

clean_protos
generate_protos
fix_headers
