#include "word_count_mapper.h"

#include <algorithm>
#include <fstream>

int main() {
  /*
  std::string jobName = std::string(argv[1]);
  int numReducer = std::stoi(argv[2]);
  int delay = std::stoi(argv[3]);
  std::string inputFilename = std::string(argv[4]);
  int chunkSize = std::stoi(argv[5]);
  std::string localityConfigFilename = std::string(argv[6]);
  std::string outputDir = std::string(argv[7]);
  */

  std::string jobName = "word_count_mapper_test";
  int numReducer = 3;
  int delay = 0;
  std::string inputFilename = "./ta/testcases/01.word";
  int chunkSize = 2;
  std::string localityConfigFilename = "./ta/testcases/01.loc";
  std::string outputDir = "./tests/";

  WordCountConfig* config = new WordCountConfig(1, 1, jobName, numReducer, delay, inputFilename, chunkSize, localityConfigFilename, outputDir);

  std::ifstream f(inputFilename);
  int lines = std::count(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>(), '\n') + 1;
  int numMapper = lines / chunkSize;

  for (int i = 1; i <= numMapper; ++i) {
    WordCountMapper* mapper = new WordCountMapper(i, i, config);

    WordCountMapper::run(mapper);

    delete mapper;
  }

  delete config;

  return 0;
}
