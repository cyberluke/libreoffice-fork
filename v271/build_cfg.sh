#!/bin/bash
export PATH=/usr/bin:/bin:$PATH
export SHELL=/bin/sh
cd /d/_SATIN_AI/LibreOffice
touch config_host.mk.stamp
make MAKE_RESTARTS=1 officecfg postprocess > /d/_SATIN_AI/LibreOffice/v271/build_cfg.log 2>&1
echo "CFG_BUILD_EXIT=$?" >> /d/_SATIN_AI/LibreOffice/v271/build_cfg.log