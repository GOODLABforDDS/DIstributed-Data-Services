#!/bin/bash
set -xe

SCRIPTPATH="$( cd "$(dirname "$0")" ; pwd -P )"
SRCPATH=$(cd $SCRIPTPATH/..; pwd -P)
NPROC=$(nproc || grep -c ^processor /proc/cpuinfo)

BUILD_DIR="$SRCPATH/build"
mkdir -p $BUILD_DIR && cd $BUILD_DIR
cmake "$SRCPATH" \
    -DENABLE_TESTS=ON \
    -DDOWNLOAD_GRPC_CN=ON
make -j $NPROC

# run test exe
/eraft/build/gtest_example_tests
/eraft/build/eraftkv_server_test
/eraft/build/rocksdb_storage_impl_tests
/eraft/build/log_entry_cache_tests
/eraft/build/log_entry_cache_tests
/eraft/build/google_example_banchmark
/eraft/build/log_entry_cache_benchmark
/eraft/build/eraftkv
