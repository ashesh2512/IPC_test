#!/bin/bash -l
#SBATCH -J mpi_test
#SBATCH -N 1 
#SBATCH -t 00:10:00
#SBATCH -A VEN114
#SBATCH --output=job.out

monitordir=logs
mkdir $monitordir
./compile.sh
sleep 5

module purge
module load cpe/25.09 PrgEnv-amd amd/6.4.1 rocm/6.4.1
module load craype-x86-trento
module load craype-accel-amd-gfx90a
module unload darshan-runtime
export LD_LIBRARY_PATH=$CRAY_LD_LIBRARY_PATH:$LD_LIBRARY_PATH

export MPICH_GPU_SUPPORT_ENABLED=1
export MPICH_GPU_IPC_ENABLED=1
export GTL_DISABLE_HSA_IPC_SIGNAL_CACHE=0
export MPICH_GPU_IPC_CACHE_MAX_SIZE=1
#export MPICH_GPU_IPC_THRESHOLD=0
#export GTL_HSA_VSMSG_CUTOFF_SIZE=0
#export MPICH_GPU_A_A_MEMCPY_THRESHOLD=0
# export GTL_VERBOSE=1
# export GTL_VERBOSE2=1

module list

nodelist="nodelist"
srun /usr/bin/hostname &> $nodelist
echo "Running on the following nodes :"
cat $nodelist

mem_pids=()
sim_pids=()

while IFS="" read -r p || [ -n "$p" ]; do
  {
    while true; do
      srun -N1 -n1 -c1 --exact --unbuffered --nodelist=$p --tasks-per-node=1 --cpus-per-task=1 --threads-per-core=1 amd-smi metric --mem-usage --csv | column -t -s ',' >> ${monitordir}/${p}_mem.csv
      sleep 0.1
    done
  } &
  mpid=$!
  mem_pids+=("$mpid")
  echo "Launched memory monitor on -- $p -- with pid $mpid "
done < $nodelist

ldd mpi_test
srun -N1 -n2 -c1 --exact --tasks-per-node=2 --cpus-per-task=1 --threads-per-core=1 --gpus-per-task=1 --gpu-bind=closest --output ${monitordir}/test.log ./mpi_test &
spid=$!
sim_pids+=("$spid")

for pid in "${sim_pids[@]}"; do
  wait "$pid"
done

for pid in "${mem_pids[@]}"; do
  kill -9 "$pid"
  echo "Job with PID $pid killed"
done

