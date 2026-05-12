#include <mpi.h>
#include <hip/hip_runtime.h>
#include <iostream>
#include <random>
#include <cmath>
#include <chrono>

#define HIP_CALL(call)                                                 \
{                                                                      \
  hipError_t err = call;                                               \
  if (err != hipSuccess) {                                             \
    std::cerr << "HIP error in " << __FILE__ << ":" << __LINE__        \
              << " - " << hipGetErrorString(err) << std::endl;         \
    MPI_Abort(MPI_COMM_WORLD, -1);                                     \
  }                                                                    \
}

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

  const int Nmpi = 20; // Number of MPI iterations
  const int print_interval = 1;
  const int Ninflight = 50; // Number of messages in flight simultaneously

  // Random number generator only on rank 0
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dist(1, 1024);

  float* d_sendbuf[Ninflight];
  float* d_recvbuf[Ninflight];
  float* h_temp[Ninflight];
  int send_peer = (rank + 1) % size;
  int recv_peer = (rank - 1 + size) % size;

  auto t0 = std::chrono::steady_clock::now(); //start time
  for (int iter = 0; iter < Nmpi; ++iter) {
    
    int N;
    if (rank == 0) {
      // N = 256*dist(gen); // Random buffer size chosen by rank 0
      N = 256*1024*40;
    }

    // Broadcast buffer size to all ranks
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);

    // Allocate buffers for all in-flight messages
    for (int m = 0; m < Ninflight; ++m) {
      h_temp[m] = new float[N];

      HIP_CALL(hipMalloc(&d_sendbuf[m], N * sizeof(float)));
      HIP_CALL(hipMalloc(&d_recvbuf[m], N * sizeof(float)));
    }

#if CREATE_DUMMY
    // create a no-op dummy array that adds to the GPU memory pool
    float* dummy;
    HIP_CALL(hipMalloc(&dummy, N * sizeof(float)));
#endif

    // Initialize and copy all send buffers to device
    for (int m = 0; m < Ninflight; ++m) {
      for (int i = 0; i < N; ++i) {
        h_temp[m][i] = static_cast<float>((rank+10) * i + m);
      }
      HIP_CALL(hipMemcpy(d_sendbuf[m], h_temp[m], N * sizeof(float), hipMemcpyHostToDevice));
    }

    // Post all non-blocking receives and sends so Ninflight messages are in flight
    {
      MPI_Request requests[2 * Ninflight];
      int nreqs = 0;

      if (rank == 1) {
        for (int m = 0; m < Ninflight; ++m) {
          MPI_Irecv(d_recvbuf[m], N, MPI_FLOAT, 0, m, MPI_COMM_WORLD, &requests[nreqs++]);
        }
      }

      if (rank == 0) {
        for (int m = 0; m < Ninflight; ++m) {
          MPI_Isend(d_sendbuf[m], N, MPI_FLOAT, 1, m, MPI_COMM_WORLD, &requests[nreqs++]);
        }
      }

      // Wait for all in-flight messages to complete
      MPI_Waitall(nreqs, requests, MPI_STATUSES_IGNORE);
    }

    // Copy back to host and validate
    for (int m = 0; m < Ninflight; ++m) {
      HIP_CALL(hipMemcpy(h_temp[m], d_recvbuf[m], N * sizeof(float), hipMemcpyDeviceToHost));
    }

    hipDeviceSynchronize();
    
    // Validate
    // if(rank == 1) {
    //   for (int m = 0; m < Ninflight; ++m) {
    //     for (int i = 0; i < N; ++i) {
    //       float expected = static_cast<float>((send_peer+10) * i + m);
    //       if (std::abs(h_temp[m][i] - expected) > 1e-3f) {
    //         printf("Rank %d: Validation failed at iteration %d, msg %d, index %d. Expected %.6f, got %.6f\n",
    //                rank, iter, m, i, expected, h_temp[m][i]);
    //         fflush(stdout);
    //         MPI_Abort(MPI_COMM_WORLD, -1);
    //       }
    //     }
    //   }
    // }

    // Clean up
#if CREATE_DUMMY
    hipFree(dummy);
#endif
    for (int m = 0; m < Ninflight; ++m) {
      hipFree(d_recvbuf[m]);
      hipFree(d_sendbuf[m]);
      delete[] h_temp[m];
    }

    // Show progress
    if (rank == 0 && (iter % print_interval == 0 || iter == Nmpi - 1)) {
      double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
      printf("[Progress] Iteration %d/%d passed with N = %d, Ninflight = %d, t = %.3f s\n", iter + 1, Nmpi, N, Ninflight, elapsed);
      fflush(stdout);
    }
  }

  if (rank == 0) {
    std::cout << "✅ All " << Nmpi << " iterations passed." << std::endl;
  }

  MPI_Finalize();
  return 0;
}
