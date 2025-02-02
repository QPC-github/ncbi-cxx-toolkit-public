# $Id$

REQUIRES = objects algo

ASN_DEP = seq

APP = dustmasker
SRC = main dust_mask_app

LIB = xalgodustmask seqmasks_io $(OBJREAD_LIBS) xobjutil \
      seqdb blastdb $(OBJMGR_LIBS:%=%$(STATIC)) $(LMDB_LIB)

 CPPFLAGS = $(ORIG_CPPFLAGS) $(BLAST_THIRD_PARTY_INCLUDE)
LIBS = $(BLAST_THIRD_PARTY_LIBS) $(CMPRS_LIBS) $(NETWORK_LIBS) $(DL_LIBS) $(ORIG_LIBS)

CXXFLAGS = $(FAST_CXXFLAGS)
LDFLAGS  = $(FAST_LDFLAGS)


WATCHERS = camacho fongah2
