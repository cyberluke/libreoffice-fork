#!/bin/bash
export PATH=/usr/bin:/bin:$PATH
export SHELL=/bin/sh
cd /d/_SATIN_AI/LibreOffice
touch config_host.mk.stamp
touch config_host_lang.mk.stamp
make MAKE_RESTARTS=1 postprocess > /d/_SATIN_AI/LibreOffice/v271/build_postprocess.log 2>&1
echo "POSTPROCESS_BUILD_EXIT=$?" >> /d/_SATIN_AI/LibreOffice/v271/build_postprocess.log