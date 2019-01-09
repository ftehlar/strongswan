
SUBDIRS = test

ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libvpp-agent-api.la

AM_CFLAGS = -I$(top_srcdir)/models -I$(top_srcdir)/configurator -Wall -Wextra -Og

libvpp_agent_api_la_SOURCES = \
	configurator/configurator.grpc-c.c \
	vaa_json.c vaa_json.h \
	nxjson.c nxjson.h \
	models/linux/linux.grpc-c.c \
	models/linux/interfaces/interface.grpc-c.c \
	models/linux/l3/route.grpc-c.c \
	models/linux/namespace/namespace.grpc-c.c \
	models/linux/l3/arp.grpc-c.c \
	models/vpp/interfaces/state.grpc-c.c \
	models/vpp/vpp.grpc-c.c \
	models/vpp/acl/acl.grpc-c.c \
	models/vpp/interfaces/interface.grpc-c.c \
	models/vpp/l2/bridge-domain.grpc-c.c \
	models/vpp/l2/fib.grpc-c.c \
	models/vpp/l2/xconnect.grpc-c.c \
	models/vpp/ipsec/ipsec.grpc-c.c \
	models/vpp/l3/arp.grpc-c.c \
	models/vpp/l3/l3.grpc-c.c \
	models/vpp/l3/route.grpc-c.c \
	models/vpp/nat/nat.grpc-c.c \
	models/vpp/punt/punt.grpc-c.c \
	models/vpp/stn/stn.grpc-c.c


libvpp_agent_api_la_LIBADD = -lgpr -lgrpc -lprotobuf-c -lprotobuf -lprotobuf-c

bin_PROGRAMS = grpc_demo_setup

grpc_demo_setup_SOURCES = grpc_demo_setup.c

grpc_demo_setup_LDADD = -lvpp-agent-api -lgrpc -lgrpc-c -lprotobuf -lprotobuf \
		-lhiredis
