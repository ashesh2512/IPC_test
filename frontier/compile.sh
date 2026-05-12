#!/bin/bash -l

rm -rf mpi_test logs

unset LD_LIBRARY_PATH
module purge
#module load cpe/26.03 PrgEnv-amd amd/7.2.0 rocm/7.2.0
module load cpe/26.03 PrgEnv-amd amd/7.0.2 rocm/7.0.2
module load cray-mpich/9.1.0
#module load cpe/25.09 PrgEnv-amd amd/6.4.2 rocm/6.4.2
#module load cray-mpich/9.0.1
module load craype-x86-trento
module load craype-accel-amd-gfx90a
module unload darshan-runtime
module list

export LD_LIBRARY_PATH=$CRAY_LD_LIBRARY_PATH:$LD_LIBRARY_PATH
export MPICH_GPU_SUPPORT_ENABLED=1
#CC -std=c++17 -Wno-unused-result mpi_test.cpp -o mpi_test
CC -std=c++17 -Wno-unused-result mpi_test_mult_msgs.cpp -o mpi_test
