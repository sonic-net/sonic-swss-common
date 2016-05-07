#!/bin/bash -ex

git clone https://github.com/google/googletest.git
cd googletest
mkdir build
cd build
cmake ..
cmake --build .
exit 0
