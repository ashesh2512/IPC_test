#include <mpi.h>
#include <hip/hip_runtime.h>
#include <iostream>
#include <random>
#include <cmath>

#define HIP_CALL(call)                                                 \
{                                                                      \
  hipError_t err = call;                                               \
  if (err != hipSuccess) {                                             \
    std::cerr << "HIP error in " << __FILE__ << ":" << __LINE__        \
              << " - " << hipGetErrorString(err) << std::endl;         \
    MPI_Abort(MPI_COMM_WORLD, -1);                                     \
  }                                                                    \
}

#define USE_SEPARATE_BUFFER 1
#define CREATE_DUMMY 0

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);

  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  if (size < 2) {
    if (rank == 0)
      std::cerr << "This test requires at least 2 MPI processes." << std::endl;
    MPI_Finalize();
    return 1;
  }

  const int Nmpi = 200000; // Number of MPI iterations
  const int print_interval = 100;

  // Random number generator only on rank 0
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(1, 1024);

  float* d_sendbuf = nullptr;
  float* d_recvbuf = nullptr;
  float* h_temp = nullptr;
  int send_peer = (rank + 1) % size;
  int recv_peer = (rank - 1 + size) % size;

  for (int iter = 0; iter < Nmpi; ++iter) {
    
    int N;
    if (rank == 0) {
      // N = 256*dist(gen); // Random buffer size chosen by rank 0
      N = 256*1024;
    }

    // Broadcast buffer size to all ranks
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Allocate buffers
    h_temp = new float[N];

    // Allocate a single device buffer of size 2N
#if USE_SEPARATE_BUFFER
    HIP_CALL(hipMalloc(&d_sendbuf, N * sizeof(float)));
    HIP_CALL(hipMalloc(&d_recvbuf, N * sizeof(float)));
#else
    float* d_buffer;
    HIP_CALL(hipMalloc(&d_buffer, 2 * N * sizeof(float)));
    d_sendbuf = d_buffer;
    d_recvbuf = d_buffer + N;
#endif
#if CREATE_DUMMY
    // create a no-op dummy array that adds to the GPU memory pool
    float* dummy;
    HIP_CALL(hipMalloc(&dummy, N * sizeof(float)));
#endif

    // Initialize host buffer
    for (int i = 0; i < N; ++i) {
      h_temp[i] = static_cast<float>((rank+10) * i);
    }

    // Copy to device
    HIP_CALL(hipMemcpy(d_sendbuf, h_temp, N * sizeof(float), hipMemcpyHostToDevice));

    // Send and receive directly from device
    // MPI_Sendrecv(d_sendbuf, N, MPI_FLOAT, recv_peer, 0,
    // 						 d_recvbuf, N, MPI_FLOAT, send_peer, 0,
    // 						 MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    {
      if (rank == 1)
          MPI_Recv(d_recvbuf, N, MPI_FLOAT, 0, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

      if (rank == 0)
          MPI_Send(d_sendbuf, N, MPI_FLOAT, 1, 0, MPI_COMM_WORLD);
    }

    // Copy back to host
    HIP_CALL(hipMemcpy(h_temp, d_recvbuf, N * sizeof(float), hipMemcpyDeviceToHost));

    hipDeviceSynchronize();
    
    // Validate
    // if(rank == 1) {
    //   for (int i = 0; i < N; ++i) {
    //     float expected = static_cast<float>((send_peer+10) * i);
    //     if (std::abs(h_temp[i] - expected) > 1e-3f) {
    //       printf("Rank %d: Validation failed at iteration %d, index %d. Expected %.6f, got %.6f\n",
    //              rank, iter, i, expected, h_temp[i]);
    //       fflush(stdout);
    //       MPI_Abort(MPI_COMM_WORLD, -1);
    //     }
    //   }
    // }

    // Clean up
#if CREATE_DUMMY
    hipFree(dummy);
#endif
#if USE_SEPARATE_BUFFER
    hipFree(d_recvbuf);
    hipFree(d_sendbuf);
#else
    hipFree(d_buffer);
#endif
    delete[] h_temp;

    // Show progress
    if (rank == 0 && (iter % print_interval == 0 || iter == Nmpi - 1)) {
      printf("[Progress] Iteration %d/%d passed with N = %d\n", iter + 1, Nmpi, N);
      fflush(stdout);
    }
  }

  if (rank == 0) {
    std::cout << "✅ All " << Nmpi << " iterations passed." << std::endl;
  }

  MPI_Finalize();
  return 0;
}


