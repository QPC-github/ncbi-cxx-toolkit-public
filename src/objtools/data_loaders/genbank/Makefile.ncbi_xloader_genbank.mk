# $Id$

GENBANK_THIRD_PARTY_LIBS        = $(PSG_CLIENT_LIBS)
GENBANK_THIRD_PARTY_STATIC_LIBS = $(PSG_CLIENT_STATIC_LIBS)
GENBANK_PSG_CLIENT_LIB          = psg_client
GENBANK_PSG_CLIENT_LDEP         = $(GENBANK_PSG_CLIENT_LIB) xconnserv xxconnect2

REQUIRES = PSGLoader LIBUV NGHTTP2
