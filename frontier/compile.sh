#!/bin/bash -l

module purge
module load cpe/24.11 PrgEnv-amd craype-accel-amd-gfx90a amd rocm
module load craype-x86-trento
module unload darshan-runtime
export LD_LIBRARY_PATH=$CRAY_LD_LIBRARY_PATH:$LD_LIBRARY_PATH
export MPICH_GPU_SUPPORT_ENABLED=1

CC -std=c++17 -Wno-unused-result mpi_test.cpp -o mpi_test

