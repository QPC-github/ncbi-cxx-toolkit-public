# $Id$

APP = ctl_sp_who_ftds100
SRC = ctl_sp_who_ftds100 dbapi_driver_sample_base_ftds100

LIB  = ncbi_xdbapi_ftds100$(STATIC) $(FTDS100_CTLIB_LIB) \
       dbapi_driver$(STATIC) $(XCONNEXT) xconnect xncbi
LIBS = $(FTDS100_CTLIB_LIBS) $(NETWORK_LIBS) $(ORIG_LIBS) $(DL_LIBS)

CPPFLAGS = -DFTDS_IN_USE -I$(includedir)/dbapi/driver/ftds100 \
           $(FTDS100_INCLUDE) $(ORIG_CPPFLAGS)

CHECK_REQUIRES = in-house-resources
# CHECK_CMD = run_sybase_app.sh ctl_sp_who_ftds100
CHECK_CMD = run_sybase_app.sh ctl_sp_who_ftds100 -S MSDEV1 /CHECK_NAME=ctl_sp_who_ftds100-MS
# CHECK_CMD = run_sybase_app.sh ctl_sp_who_ftds100 -S MS2008DEV1 /CHECK_NAME=ctl_sp_who_ftds100-MS2008

WATCHERS = ucko satskyse
