#!/bin/bash

# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

sudo apt update -y    
sudo apt install -y git zip unzip build-essential wget curl libpng-dev autoconf libtool pkg-config libcurl4-openssl-dev libssl-dev uuid-dev zlib1g-dev libpulse-dev software-properties-common coreutils jq cmake
sudo apt install -y python3.8 python3-venv python3.8-venv
sudo apt-get install python3-pip
sudo pip3 install git+https://github.com/aws-greengrass/aws-greengrass-gdk-cli.git@v1.1.0

MY_GG_COMPONENT_DIR=`pwd`
pushd $MY_GG_COMPONENT_DIR/lookoutvision-inference-src
bash buildscript.sh
mv inference.tar.gz $MY_GG_COMPONENT_DIR/aws.iot.lookoutvision.inference/.
popd $MY_GG_COMPONENT_DIR

## Update GG Component Version
cd $MY_GG_COMPONENT_DIR/aws.iot.lookoutvision.inference
REVISION_VER=$(cat revision) && NEXT_REV=$((REVISION_VER+1))
echo $NEXT_REV > revision
export VERSION=$(cat version)
COMPLETE_VER="$VERSION.$REVISION_VER" && VER=${COMPLETE_VER}
echo Building component version $VER
jq -r --arg VER "$VER" '.component[].version=$VER' gdk-config.json > gdk-config.json.bak
mv gdk-config.json.bak gdk-config.json
gdk component build
gdk component publish
cd ..