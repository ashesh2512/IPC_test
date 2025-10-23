#!/bin/bash -l

rm -rf mpi_test

unset LD_LIBRARY_PATH
module load cpe/25.09 PrgEnv-amd amd/6.4.1 rocm/6.4.1
module load craype-x86-trento
module load craype-accel-amd-gfx90a
module unload darshan-runtime

export LD_LIBRARY_PATH=$CRAY_LD_LIBRARY_PATH:$LD_LIBRARY_PATH
export MPICH_GPU_SUPPORT_ENABLED=1
CC -std=c++17 -Wno-unused-result mpi_test.cpp -o mpi_test

# export LD_LIBRARY_PATH=/home/users/sharmaas/test_mpi/rocm_64_lib:/home/users/sharmaas/test_mpi/mpi_lib:/home/users/sharmaas/test_mpi/gtl_lib:$LD_LIBRARY_PATH
# export MPICH_GPU_SUPPORT_ENABLED=1
# CC -std=c++17 -Wno-unused-result --enable-new-dtags mpi_test.cpp -o mpi_test
