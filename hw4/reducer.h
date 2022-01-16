#ifndef _REDUCER_H_
#define _REDUCER_H_

#include <fstream>
#include <iostream>
#include <vector>

#include "mapper.h"
#include "types.h"

class Reducer {
private:
  // the ID of the reducer, range from 1 ~ config->numReducers
  int id;
  // the task ID of the reducer, range from 0 ~ config->numReducers-1,
  // for matching the partition key
  int taskId;

  Config* config;

  CallbackFunc callback;

  std::vector<ListKV*>* fetch() {
    std::vector<ListKV*>* output = new std::vector<ListKV*>;
    output->reserve(config->numMappers);

    for (int i = 1; i <= config->numMappers; ++i) {
      std::string filename = config->mapperOutFilename(i);
      std::ifstream f(filename);

      // the partition hash will be in the range of 0 ~ config->numReducers-1
      int partitionId;
      std::string key;
      int value;

      ListKV* data = new ListKV;

      while (f >> partitionId >> key >> value) {
        if (partitionId == taskId) {
          data->emplace_back(key, value);
        }
      }

      output->emplace_back(data);
    }

    return output;
  }

  VectorKV* merge(std::vector<ListKV*>* input) {
    int len = (int)input->size();

    for (int i = 1; i < len; i <<= 1) {
      for (int j = 0; j + i < len; j += (i << 1)) {
        // inplace merge list[j] and list[j + i] into list[j]

        ListKV* lhs = input->at(j);
        ListKV* rhs = input->at(j + i);

        ListKV::iterator rit = rhs->begin();

        for (ListKV::iterator lit = lhs->begin(); lit != lhs->end(); ++lit) {
          while (rit != rhs->end() && (*rit) < (*lit)) {
            lhs->emplace(lit, *rit);
            ++rit;
          }
        }

        while (rit != rhs->end()) {
          lhs->emplace(lhs->end(), *rit);
          ++rit;
        }

        delete rhs;
      }
    }

    ListKV* l = input->at(0);
    VectorKV* output = new VectorKV(l->begin(), l->end());

    delete l;
    delete input;

    return output;
  }

  VectorKVs* group(VectorKV* input) {
    VectorKVs* output = new VectorKVs;

    for (const KV& kv : (*input)) {
      if (output->empty() || output->back().compare(kv) != 0) {
        output->emplace_back(kv.key);
      }
      output->back().values.emplace_back(kv.value);
    }

    delete input;

    return output;
  }

  VectorKV* reduce(VectorKVs* input) {
    VectorKV* output = new VectorKV;
    output->reserve(input->size());

    for (const KVs& kvs : (*input)) {
      // reduce by sum aggregation
      int sum = 0;
      for (int value : kvs.values) {
        sum += value;
      }
      output->emplace_back(kvs.key, sum);
    }

    delete input;

    return output;
  }

  void write(VectorKV* input) {
    std::string filename = config->reducerOutFilename(taskId);
    std::ofstream f(filename);

    for (const KV& kv : (*input)) {
      f << kv.key << ' ' << kv.value << '\n';
    }

    delete input;
  }

public:
  Reducer(int id, int taskId, Config* config, CallbackFunc callback)
    : id(id), taskId(taskId), config(config), callback(callback) {}

  static void* run(void* arg) {
    Reducer* reducer = (Reducer*)arg;

    std::vector<ListKV*>* fetchResult = reducer->fetch();
    // std::cout << "fetch done" << std::endl;

    VectorKV* mergeResult = reducer->merge(fetchResult);
    // std::cout << "merge done" << std::endl;

    VectorKVs* groupResult = reducer->group(mergeResult);
    // std::cout << "group done" << std::endl;

    VectorKV* reduceResult = reducer->reduce(groupResult);
    // std::cout << "reduce done" << std::endl;

    reducer->write(reduceResult);
    // std::cout << "write done" << std::endl;

    (*(reducer->callback))(MessageType::REDUCE_DONE, reducer->id, reducer->taskId, 0);

    delete reducer;

    return nullptr;
  }
};

#endif  // _REDUCER_H_
