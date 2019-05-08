#!/usr/bin/env bash

protoc --grpc_out=. -I. --plugin=protoc-gen-grpc=`which grpc_cpp_plugin` ssipsec.proto
protoc --cpp_out=. -I. ssipsec.proto
