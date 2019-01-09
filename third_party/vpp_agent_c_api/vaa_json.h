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

/** Converts protobuf-c message to JSON format and back */

#ifndef _VPP_AGENT_API_JSON__H_
#define _VPP_AGENT_API_JSON__H_

#include <protobuf-c/protobuf-c.h>

/** convert protobuf-c message to JSON string */
char *vaa_to_json(ProtobufCMessage *msg);

/** convert JSON string to protobuf-c message */
void *vaa_to_proto(char *msg, const ProtobufCMessageDescriptor *desc);

#endif /* _VPP_AGENT_API_JSON__H_ */
