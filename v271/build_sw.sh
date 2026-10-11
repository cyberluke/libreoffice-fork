#!/bin/bash
export PATH=/usr/bin:/bin:$PATH
export SHELL=/bin/sh
cd /d/_SATIN_AI/LibreOffice
make MAKE_RESTARTS=1 sw > /d/_SATIN_AI/LibreOffice/v271/build_sw.log 2>&1
echo "SW_BUILD_EXIT=$?" >> /d/_SATIN_AI/LibreOffice/v271/build_sw.log