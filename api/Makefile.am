
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libkiknos-api.la libkiknos-api-wrapper.la

AM_CXXFLAGS = `pkg-config --cflags protobuf grpc`

libkiknos_api_la_SOURCES = ssipsec.grpc.pb.cc ssipsec.grpc.pb.h \
                           ssipsec.pb.cc ssipsec.pb.h

libkiknos_api_la_LDFLAGS = -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc`\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl -avoid-version

libkiknos_api_la_LIBADD = -lgrpc -lprotobuf

libkiknos_api_wrapper_la_SOURCES = kiknos_api_wrapper.cc kiknos_api_wrapper.h

libkiknos_api_wrapper_la_LDFLAGS = -L/usr/local/lib `pkg-config --libs protobuf grpc++ grpc`\
           -Wl,--no-as-needed -lgrpc++_reflection -Wl,--as-needed\
           -ldl -avoid-version

libkiknos_api_wrapper_la_LIBADD = $(top_builddir)/libkiknos-api.la
