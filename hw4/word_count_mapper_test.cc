#include <algorithm>
#include <fstream>

#include "word_count_mapper.h"

void callback(int id, int taskId) {
  printf("callback from id %d done task %d\n", id, taskId);
}

int main() {
  /*
  std::string jobName = std::string(argv[1]);
  int numReducers = std::stoi(argv[2]);
  int delay = std::stoi(argv[3]);
  std::string inputFilename = std::string(argv[4]);
  int chunkSize = std::stoi(argv[5]);
  std::string localityConfigFilename = std::string(argv[6]);
  std::string outputDir = std::string(argv[7]);
  */

  std::string jobName = "word_count_mapper_test";
  int numReducers = 3;
  int delay = 0;
  std::string inputFilename = "./ta/testcases/01.word";
  int chunkSize = 2;
  std::string localityConfigFilename = "./ta/testcases/01.loc";
  std::string outputDir = "./tests/";

  WordCountConfig* config = new WordCountConfig(1, 1, jobName, numReducers, delay, inputFilename, chunkSize, localityConfigFilename, outputDir);

  for (int i = 1; i <= config->numMappers; ++i) {
    WordCountMapper* mapper = new WordCountMapper(i, i, i, config, &callback);

    WordCountMapper::run((void*)mapper);
  }

  delete config;

  return 0;
}
