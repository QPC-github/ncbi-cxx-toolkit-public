# $Id$

APP = test_ncbi_ftp_connector
SRC = test_ncbi_ftp_connector
LIB = connect $(NCBIATOMIC_LIB)

LIBS = $(NETWORK_LIBS) $(ORIG_LIBS)
LINK = $(C_LINK)

CHECK_CMD  = test_ncbi_ftp_connector.sh /CHECK_NAME=test_ncbi_ftp_connector
CHECK_COPY = test_ncbi_ftp_connector.sh

WATCHERS = lavr
