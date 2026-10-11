#!/bin/bash
export PATH=/usr/bin:/bin:$PATH
export SHELL=/bin/sh
cd /d/_SATIN_AI/LibreOffice
make MAKE_RESTARTS=1 officelabs > /d/_SATIN_AI/LibreOffice/v271/build_officelabs.log 2>&1
echo "OFFICELABS_BUILD_EXIT=$?" >> /d/_SATIN_AI/LibreOffice/v271/build_officelabs.log