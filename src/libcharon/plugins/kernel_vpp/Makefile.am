AM_CPPFLAGS = \
    -I${linux_headers} \
    -I$(top_srcdir)/src/libstrongswan \
    -I$(top_srcdir)/src/libcharon \
    -I$(top_srcdir)/third_party/vpp_agent_c_api \
    -I$(top_srcdir)/third_party/vpp_agent_c_api/models

AM_CFLAGS = \
    $(PLUGIN_CFLAGS)

if MONOLITHIC
noinst_LTLIBRARIES = libstrongswan-kernel-vpp.la
else
plugin_LTLIBRARIES = libstrongswan-kernel-vpp.la
endif

libstrongswan_kernel_vpp_la_SOURCES = \
    kernel_vpp_plugin.h kernel_vpp_plugin.c \
    kernel_vpp_net.h kernel_vpp_net.c \
    kernel_vpp_grpc.h kernel_vpp_grpc.c \
    kernel_vpp_ipsec.h kernel_vpp_ipsec.c

libstrongswan_kernel_vpp_la_LDFLAGS = -module -avoid-version \
	-L$(top_srcdir)/third_party/grpc-c/build/lib

libstrongswan_kernel_vpp_la_LIBADD = -lgrpc -lgpr -lgrpc-c -lprotobuf-c \
                                     -lprotobuf -lvpp-agent-api -lhiredis
