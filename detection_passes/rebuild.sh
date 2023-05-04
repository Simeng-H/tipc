#! /bin/zsh

# get dir of this script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
echo "Script directory: $SCRIPT_DIR"

# get dir of build
BUILD_DIR=$SCRIPT_DIR/build

# remove all in build dir
rm -rf $BUILD_DIR/*

# cd to build dir
cd $BUILD_DIR
cmake ..
make -j4