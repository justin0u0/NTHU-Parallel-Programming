#include <cstdlib>
#include <mpi.h>

#include "job_tracker.h"
#include "task_tracker.h"

int main(int argc, char** argv) {
  MPI_Init(&argc, &argv);
  int rank, size;
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &size);

  cpu_set_t cpuset;
  sched_getaffinity(0, sizeof(cpuset), &cpuset);

  int nodes = size;
  int cpus = CPU_COUNT(&cpuset);
  std::string jobName = std::string(argv[1]);
  int numReducers = std::stoi(argv[2]);
  int delay = std::stoi(argv[3]);
  std::string inputFilename = std::string(argv[4]);
  int chunkSize = std::stoi(argv[5]);
  std::string localityConfigFilename = std::string(argv[6]);
  std::string outputDir = std::string(argv[7]);

  Config* config = new Config(nodes, cpus, jobName, numReducers, delay, inputFilename, chunkSize, localityConfigFilename, outputDir);

  if (rank == 0) {
    JobTracker* jobTracker = new JobTracker(config);

    jobTracker->run();

    delete jobTracker;
  } else {
    TaskTracker* taskTracker = new TaskTracker(rank, config);

    taskTracker->run();

    delete taskTracker;
  }

  MPI_Finalize();

  delete config;

  return 0;
}
