#!/bin/bash -l
#SBATCH -J mpi_test
#SBATCH -N 1 
#SBATCH -t 00:10:00
#SBATCH -A VEN114
#SBATCH --output=job.out

monitordir=logs
mkdir $monitordir

module purge
#module load cpe/26.03 PrgEnv-amd amd/7.2.0 rocm/7.2.0
module load cpe/26.03 PrgEnv-amd amd/7.0.2 rocm/7.0.2
module load cray-mpich/9.1.0
#module load cpe/25.09 PrgEnv-amd amd/6.4.2 rocm/6.4.2
#module load cray-mpich/9.0.1
module load craype-x86-trento
module load craype-accel-amd-gfx90a
module unload darshan-runtime
export LD_LIBRARY_PATH=$CRAY_LD_LIBRARY_PATH:$LD_LIBRARY_PATH

export MPICH_GPU_SUPPORT_ENABLED=1
export MPICH_GPU_IPC_ENABLED=1
export GTL_ENABLE_HSA_IPC_SIGNAL_CACHE=1
export GTL_DISABLE_HSA_IPC_SIGNAL_CACHE=0
export HSA_ENABLE_IPC_MODE_LEGACY=1
#export GTL_HSA_VSMSG_CUTOFF_SIZE=0
export MPICH_GPU_IPC_CACHE_MAX_SIZE=10

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
      #srun -N1 -n1 -c1 --exact --unbuffered --nodelist=$p --tasks-per-node=1 --cpus-per-task=1 --threads-per-core=1 amd-smi metric --mem-usage --csv | column -t -s ',' >> ${monitordir}/${p}_mem.csv
      srun -N1 -n1 -c1 --exact --unbuffered --nodelist=$p --tasks-per-node=1 --cpus-per-task=1 --threads-per-core=1 rocm-smi | grep -E "^[02]\b" >> ${monitordir}/${p}_mem.csv
      echo >> ${monitordir}/${p}_mem.csv
      sleep 0.1
    done
  } &
  mpid=$!
  mem_pids+=("$mpid")
  echo "Launched memory monitor on -- $p -- with pid $mpid "
done < $nodelist

ldd mpi_test
start=$SECONDS
srun -N1 -n2 --exact --tasks-per-node=2 --cpus-per-task=14 --threads-per-core=1 --gpus-per-task=2 --gpu-bind=closest --output ${monitordir}/test.log ./mpi_test &
spid=$!
sim_pids+=("$spid")

for pid in "${sim_pids[@]}"; do
  wait "$pid"
done
elapsed=$(( SECONDS - start ))
echo "Elapsed time: ${elapsed} seconds"

for pid in "${mem_pids[@]}"; do
  kill -9 "$pid"
  echo "Job with PID $pid killed"
done

