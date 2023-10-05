#!/bin/bash

PATCH_DIR=$(dirname "$0")
cd $PATCH_DIR
ALL_PATCHES=(./*.patch)
cd ../gem5
for PATCH in ${ALL_PATCHES[@]}; do
    git apply "../gem5-patches/$PATCH"
done
