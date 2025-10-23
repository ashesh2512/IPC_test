#!/bin/bash -l
#SBATCH -J mpi_test
#SBATCH -N 1 
#SBATCH -t 00:10:00
#SBATCH -A VEN114
#SBATCH --output=job.out

rm -rf logs
./compile.sh
sleep 10

module purge
module load cpe/24.11 PrgEnv-amd craype-accel-amd-gfx90a amd rocm
module load craype-x86-trento
module unload darshan-runtime
export LD_LIBRARY_PATH=$CRAY_LD_LIBRARY_PATH:$LD_LIBRARY_PATH
export MPICH_GPU_SUPPORT_ENABLED=1
export MPICH_GPU_IPC_ENABLED=1

nodelist="nodelist"
srun /usr/bin/hostname &> $nodelist
echo "Running on the following nodes :"
cat $nodelist

monitordir=logs
mkdir -p ${monitordir}
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
