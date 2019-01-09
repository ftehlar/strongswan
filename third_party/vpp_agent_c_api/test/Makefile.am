
# Copyright (c) 2019 Cisco and/or its affiliates.
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

TESTS = test_proto_to_json \
		test_json_to_proto

check_PROGRAMS = test_proto_to_json \
				 test_json_to_proto

AM_CLAGS = -Og

test_json_to_proto_SOURCES = \
	vaa_test.pb-c.c vaa_test.pb-c-h \
	test_json_to_proto.c

test_proto_to_json_SOURCES = \
	vaa_test.pb-c.c vaa_test.pb-c-h \
	test_proto_to_json.c

test_json_to_proto_LDADD = $(LIBPROTOBUF_C_LIBS) ../libvpp-agent-api.la \
					   -lgrpc-c -lgrpc
test_proto_to_json_LDADD = $(LIBPROTOBUF_C_LIBS) ../libvpp-agent-api.la \
					   -lgrpc-c -lgrpc

vaa_test.pb-c.c: vaa_test.proto
	@PROTOC_C@ --c_out=. -I. vaa_test.proto
