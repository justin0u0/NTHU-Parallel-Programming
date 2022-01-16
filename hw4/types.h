#ifndef _TYPES_H_
#define _TYPES_H_

#include <list>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

/* Config */

class Config {
public:
  // Number of nodes, specified by TA. One of the nodes will act as the
  // jobtracker, and the rest of the nodes will act as the tasktrackers.
  int nodes;

  // Number of CPUs allocated on a node, specified by TA. Each tasktracker
  // will create CPUS-1 mapper threads for processing mapper tasks, and
  // one reducer thread for processing reducer tasks. More threads can be
  // created by a tasktracker or a jobtracker as needed. Hence, the total
  // number of threads on a node can exceed the number of CPUs allocated
  // on a node.
  int cpus;

  // The name of the job. It is used to generate the output filename.
  std::string jobName;

  // The number of reducer tasks of the MapReduce program.
  int numReducers;

  // The number of mapper tasks of the MapReduce program. The number of
  // mapper tasks will be the same as the number of data chunks of the
  // input file specified in the file locality configure file.
  int numMappers;

  // The sleeping time in seconds for reading a remote data chunk in a
  // mapper task.
  int delay;

  // The path of the input file. Your program should read the data chunks
  // and file content from this file.
  std::string inputFilename;

  // The number of lines for a data chunk from the input file. Hence, the
  // amount of data read from an input file by the MapReduce program should
  // be the chunk size specified in the command line times the number of
  // data chunks specified in the data locality configuration file, NOT
  // necessarily the whole input file.
  int chunkSize;

  // The path of the configuration file that contains a list of mapping
  // between the chunkID and nodeID(i.e., tasktrackerID). The file should
  // be loaded into memory by the jobtracker for scheduling.
  std::string localityConfigFilename;

  // The pathname of the output directory. All the output files from your
  // program (described below) should be stored under this directory.
  std::string outputDir;

  class Locality {
  public:
    // taskId equals to chunkId
    int taskId;

    // nodeId is the node where the chunk store
    int nodeId;

    Locality(int taskId, int nodeId) : taskId(taskId), nodeId(nodeId) {}
  };

  // map taskId (chuckId) to nodeId
  typedef std::unordered_map<int, int> LocalityConfig;

  LocalityConfig localityConfig;

  Config(int nodes, int cpus, std::string& jobName, int numReducers, int delay,
    std::string& inputFilename, int chunkSize, std::string& localityConfigFilename, std::string& outputDir)
    : nodes(nodes), cpus(cpus), jobName(jobName), numReducers(numReducers), delay(delay),
      inputFilename(inputFilename), chunkSize(chunkSize), localityConfigFilename(localityConfigFilename), outputDir(outputDir) {

    numMappers = 0;

    // extend locality config file
    std::ifstream f(localityConfigFilename);

    int taskId, nodeId;
    while (f >> taskId >> nodeId) {
      // If the nodeId is larger than the number of worker nodes, mod the nodeId by the number of worker nodes.
      nodeId = nodeId % (nodes - 1);
      if (nodeId == 0) {
        nodeId = nodes - 1;
      }

      localityConfig.emplace(taskId, nodeId);

      // each line in the locality config file represents a data chuck
      ++numMappers;
    }
  }

  std::string logFilename() {
    return outputDir + jobName + "-log.out";
  }

  std::string mapperOutFilename(int taskId) {
    return outputDir + jobName + "-" + std::to_string(taskId) + ".temp";
  }
  
  std::string reducerOutFilename(int taskId) {
    return outputDir + jobName + "-" + std::to_string(taskId) + ".out";
  }
};

/* KV */

class KV {
private:
  unsigned int hashCode;
public:
  std::string key;
  int value;

  KV() {}

  KV(const std::string& key, int value) : key(key), value(value) {
    hashCode = std::hash<std::string>()(key);
  }

  bool operator < (const KV& rhs) const {
    return key.compare(rhs.key) < 0;
  }

  unsigned int hash(unsigned int modulus) const {
    return hashCode % modulus;
  }
};

typedef std::list<KV> ListKV;
typedef std::vector<KV> VectorKV;
typedef std::pair<KV, int> PairKVInt;
typedef std::multimap<KV, int> MultimapKVInt;

/* KVs */

class KVs {
public:
  std::string key;
  std::vector<int> values;

  KVs() {}

  KVs(const std::string& key) : key(key) {}

  int compare(const KV& b) {
    return key.compare(b.key);
  }
};

typedef std::vector<KVs> VectorKVs;

/* Message */

enum class MessageType : int {
  MAP,
  SHUFFLE,
  REDUCE,
  MAP_DONE,
  SHUFFLE_DONE,
  REDUCE_DONE,
  TERMINATE
};

#define MESSAGE_SIZE 4

// Message defines the message send between task tracker and job tracker
union Message {
  struct {
    MessageType type;

    // mapper/reducer ID
    int id;

    // task ID
    // - the chuck ID for the mapper
    // - the partition ID for the reducer
    int taskId;

    // aditional data carried
    int data;
  } data;

  int raw[MESSAGE_SIZE];
};

/* Callback */

typedef void (*CallbackFunc)(MessageType, int, int, int);

#endif // _TYPES_H_
