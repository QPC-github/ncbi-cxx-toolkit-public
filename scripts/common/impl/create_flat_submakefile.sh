#!/bin/sh
# $Id$

top_dir=$1
rel_dir=$2

if nawk 'BEGIN { exit 0 }' 2>/dev/null; then
    awk=nawk
else
    awk=awk
fi

exec > $top_dir/$rel_dir/Makefile.flat

cat <<EOF
# This file was generated by $0
# on `date +'%m/%d/%Y %H:%M:%S'`

builddir = $top_dir/$rel_dir
EOF

exec $awk -f `dirname $0`/create_flat_submakefile.awk subdir=$rel_dir \
    $top_dir/Makefile.flat
