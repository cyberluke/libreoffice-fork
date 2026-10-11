#!/bin/bash
export PATH=/usr/bin:/bin:$PATH
export SHELL=/bin/sh
cd /d/_SATIN_AI/LibreOffice
touch config_host.mk.stamp
# Force swlo.dll to relink with the refreshed component registration.
rm -f /d/_SATIN_AI/LibreOffice/instdir/program/swlo.dll
make MAKE_RESTARTS=1 sw > /d/_SATIN_AI/LibreOffice/v271/build_sw2.log 2>&1
echo "SW_EXIT=$?" >> /d/_SATIN_AI/LibreOffice/v271/build_sw2.log