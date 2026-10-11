#!/bin/bash
export PATH=/usr/bin:/bin:$PATH
export SHELL=/bin/sh
cd /d/_SATIN_AI/LibreOffice
make MAKE_RESTARTS=1 instsetoo_native > /d/_SATIN_AI/LibreOffice/v271/build_instsetoo.log 2>&1
echo "INSTSETOO_EXIT=$?" >> /d/_SATIN_AI/LibreOffice/v271/build_instsetoo.log
# refresh types.rdb if the module exposes a deliver target
find /d/_SATIN_AI/LibreOffice/instdir/program -name "types.rdb" -newer /d/_SATIN_AI/LibreOffice/workdir/UnoApiTarget/offapi.rdb -print >> /d/_SATIN_AI/LibreOffice/v271/build_instsetoo.log