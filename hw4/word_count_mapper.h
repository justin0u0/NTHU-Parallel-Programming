#ifndef _WORD_COUNT_MAPPER_H_
#define _WORD_COUNT_MAPPER_H_

#include <fstream>
#include <iostream>
#include <sstream>
#include <utility>

#include "word_count_types.h"

class WordCountMapper {
private:
  int id;
  int taskId;

  WordCountConfig* config;

  void (*callback)(int, int);

  std::vector<std::string>* split() {
    std::ifstream f(config->inputFilename);

    // read lines [taskId * chuckSize - 1, taskId * chuckSize + chuckSize)
    // so ignores (taskId - 1) * chuckSize lines
    for (int i = 0; i < (taskId - 1) * config->chunkSize; ++i) {
      f.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }

    std::vector<std::string>* output = new std::vector<std::string>;
    output->reserve(config->chunkSize);

    std::string line;
    for (int i = 0; i < config->chunkSize; ++i) {
      std::getline(f, line);
      std::stringstream ss(line);
      std::string word;

      while (ss >> word) {
        output->emplace_back(word);
      }
    }

    return output;
  }

  VectorWordCountKV* map(std::vector<std::string>* input) {
    VectorWordCountKV* output = new VectorWordCountKV;
    output->reserve(input->size());

    for (const std::string& s : (*input)) {
      output->emplace_back(s, 1);
    }

    delete input;

    return output;
  }

  MultimapWordCountKVInt* partition(VectorWordCountKV* input) {
    MultimapWordCountKVInt* output = new MultimapWordCountKVInt;

    for (const WordCountKV& kv : (*input)) {
      output->emplace(kv, kv.hash(config->numReducers));
    }

    delete input;

    return output;
  }

  void write(MultimapWordCountKVInt* input) {
    std::ofstream f(WordCountMapper::outputFilePath(taskId, config));

    for (const auto& kvi : (*input)) {
      f << kvi.second << ' ' << kvi.first.key << ' ' << kvi.first.value << '\n';
    }

    delete input;
  }

public:
  WordCountMapper(int id, int taskId, WordCountConfig* config, void (*callback)(int, int))
    : id(id), taskId(taskId), config(config), callback(callback) {}

  static std::string outputFilePath(int taskId, WordCountConfig* config) {
    return config->outputDir + config->jobName + "-" + std::to_string(taskId) + ".temp";
  }

  static void* run(void* arg) {
    WordCountMapper* mapper = (WordCountMapper*)arg;
    // std::cout << "[WordCountMapper::run]: " << mapper->id << " mapper start" << std::endl;

    std::vector<std::string>* splitResult = mapper->split();
    // std::cout << "[WordCountMapper::run]: " << mapper->id << " split done" << std::endl;

    VectorWordCountKV* mapResult = mapper->map(splitResult);
    // std::cout << "[WordCountMapper::run]: " << mapper->id << " map done" << std::endl;

    MultimapWordCountKVInt* partitionResult = mapper->partition(mapResult);
    // std::cout << "[WordCountMapper::run]: " << mapper->id << " partition done" << std::endl;

    mapper->write(partitionResult);
    // std::cout << "[WordCountMapper::run]: " << mapper->id << " write done" << std::endl;

    (*(mapper->callback))(mapper->id, mapper->taskId);
    // std::cout << "[WordCountMapper::run]: " << mapper->id << " callback done" << std::endl;

    delete mapper;

    return nullptr;
  }
};

#endif  // _WORD_COUNT_MAPPER_H_
