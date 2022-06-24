#!/bin/bash

# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT-0

rm -rf inference build
mkdir -p inference

MY_INSTALL_DIR=`pwd`/inference
export PATH="$MY_INSTALL_DIR/bin:$PATH"
echo Packages will be installed in $MY_INSTALL_DIR

## GRPC
echo Building grpc
GRPC_VERSION_TAG="v1.46.2"
pushd third_party/grpc
echo Switching grpc to ${GRPC_VERSION_TAG}
git fetch --all --tags
git switch --detach tags/${GRPC_VERSION_TAG}
git submodule update --init --recursive --depth 1
git reset --hard
popd
mkdir -p grpc_build
pushd grpc_build
cmake ../third_party/grpc -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR -DCMAKE_BUILD_TYPE=Release -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF -DgRPC_PROTOBUF_PROVIDER=module -DgRPC_ZLIB_PROVIDER=module -DgRPC_CARES_PROVIDER=module -DgRPC_ABSL_PROVIDER=module -DgRPC_RE2_PROVIDER=module -DgRPC_SSL_PROVIDER=module
make -j 20
make install
popd

## OPENCV
echo Building opencv
OPENCV_TAG="4.5.5"
pushd third_party/opencv
echo Switching opencv to ${OPENCV_TAG}
git fetch --all --tags
git switch --detach tags/${OPENCV_TAG}
git submodule update --init --recursive --depth 1
git reset --hard
popd
mkdir -p opencv_build
pushd opencv_build
cmake ../third_party/opencv -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR -DCMAKE_BUILD_TYPE=Release -DCMAKE_BUILD_TESTS=OFF -DBUILD_opencv_python=OFF -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF
make -j 20
make install
popd

## LFV Inference GG Component
echo Building LFV Inference GG Component
if [ -d build ]; then rm -rf build; fi
mkdir -p build
pushd build
cmake -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR ..
make -j 20
make install
popd

## ZIP the inference package
echo Building LFV Inference GG Component
tar -czf inference.tar.gz inference