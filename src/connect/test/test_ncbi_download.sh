#! /bin/sh
# $Id$

fw="`expr $$ '%' 2`"

if [ "$fw" = "1" ]; then
  CONN_FIREWALL=TRUE
  export CONN_FIREWALL
fi

$CHECK_EXEC test_ncbi_download downtest
