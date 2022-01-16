#ifndef _MAPPER_H_
#define _MAPPER_H_

#include <chrono>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <utility>

#include "types.h"

class Mapper {
private:
  int id;
  int taskId;
  int nodeId;

  Config* config;

  CallbackFunc callback;

  std::vector<std::string>* split() {
    std::ifstream f(config->inputFilename);

    // mock reading data from remote
    if (nodeId != config->localityConfig[taskId]) {
      sleep(config->delay);
    }

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

  VectorKV* map(std::vector<std::string>* input) {
    VectorKV* output = new VectorKV;
    output->reserve(input->size());

    for (const std::string& s : (*input)) {
      output->emplace_back(s, 1);
    }

    delete input;

    return output;
  }

  MultimapKVInt* partition(VectorKV* input) {
    MultimapKVInt* output = new MultimapKVInt;

    for (const KV& kv : (*input)) {
      output->emplace(kv, kv.hash(config->numReducers));
    }

    delete input;

    return output;
  }

  void write(MultimapKVInt* input) {
    std::ofstream f(config->mapperOutFilename(taskId));

    for (const auto& kvi : (*input)) {
      f << kvi.second << ' ' << kvi.first.key << ' ' << kvi.first.value << '\n';
    }

    delete input;
  }

public:
  Mapper(int id, int taskId, int nodeId, Config* config, CallbackFunc callback)
    : id(id), taskId(taskId), nodeId(nodeId), config(config), callback(callback) {}

  static void* run(void* arg) {
    Mapper* mapper = (Mapper*)arg;
    std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
    // std::cout << "[Mapper::run]: " << mapper->id << " mapper start" << std::endl;

    std::vector<std::string>* splitResult = mapper->split();
    // std::cout << "[Mapper::run]: " << mapper->id << " split done" << std::endl;

    VectorKV* mapResult = mapper->map(splitResult);
    // std::cout << "[Mapper::run]: " << mapper->id << " map done" << std::endl;
    int totalKeys = (int)mapResult->size();

    MultimapKVInt* partitionResult = mapper->partition(mapResult);
    // std::cout << "[Mapper::run]: " << mapper->id << " partition done" << std::endl;

    (*(mapper->callback))(MessageType::SHUFFLE, mapper->id, mapper->taskId, totalKeys);
    mapper->write(partitionResult);
    // std::cout << "[Mapper::run]: " << mapper->id << " write done" << std::endl;

		auto elapsed = std::chrono::system_clock::now() - start;
    int duration = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();
    (*(mapper->callback))(MessageType::MAP_DONE, mapper->id, mapper->taskId, duration);
    // std::cout << "[Mapper::run]: " << mapper->id << " callback done" << std::endl;

    delete mapper;

    return nullptr;
  }
};

#endif  // _MAPPER_H_
