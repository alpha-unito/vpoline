#!/bin/bash

CMAKE_VPOLINE=$HOME/vpoline/artifact/CMakeLists_capio_vpoline.txt
#CMAKE_SYSCALL=$HOME/vpoline/artifact/CMakeLists_capio_syscall.txt
LIBCAPIO_VPOLINE=$HOME/vpoline/artifact/libcapio_posix_vpoline.cpp
#LIBCAPIO_SYSCALL=$HOME/vpoline/artifact/libcapio_posix_syscall.cpp
CAPIO=$HOME/capio-vpoline-main
EXECUTABLE=$HOME/vpoline/artifact/data_rw
SRC_EXECUTABLE=$HOME/vpoline/artifact/io_test.cpp

echo "Compiling capio with vpoline as interception library..."
#cp $CMAKE_VPOLINE $CAPIO/capio/posix/vpoline/CMakeLists.txt
#cp $LIBCAPIO_VPOLINE $CAPIO/capio/posix/libcapio_posix.cpp
rm -rf $CAPIO/build_vpoline
mkdir -p $CAPIO/build_vpoline
cd $CAPIO/build_vpoline
cmake ..
make -j$(nproc)

#echo "Compiling capio with syscall_intercept as interception library..."
#cp $CMAKE_SYSCALL $CAPIO/posix/vpoline/CMakeLists.txt
#cp $LIBCAPIO_SYSCALL $CAPIO/capio/posix/libcapio_posix.cpp
#mkdir -p $CAPIO/build_syscall
#cd $CAPIO/build_syscall
#cmake ..
#make -j$(nproc)

################################################################################

g++ -o $EXECUTABLE $SRC_EXECUTABLE

#./capio_launch_bench.sh



