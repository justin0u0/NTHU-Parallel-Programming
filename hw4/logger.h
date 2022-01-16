#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <chrono>
#include <fstream>
#include <iostream>
#include <mutex>

class Logger {
public:
  enum class Level : int {
    DEBUG,
    INFO,
    ALL
  };
private:
  std::ostream* os;
  std::ofstream* ofs;
  std::mutex mutex;

  Level withLevel = Level::ALL;
  bool withStdout;
public:
  Logger() {
    os = new std::ostream(std::cout.rdbuf());

    withStdout = true;
  }

  Logger(const char* filename, Level level) {
    ofs = new std::ofstream(filename);

    os = new std::ostream(ofs->rdbuf());

    withStdout = false;
    withLevel = level;
  }

  Logger(const char* filename) : Logger(filename, Level::ALL) {}
  Logger(const std::string& filename) : Logger(filename.c_str()) {}
  Logger(const std::string& filename, Level level) : Logger(filename.c_str(), level) {}

  ~Logger() {
    delete os;
    delete ofs;
  }

  std::ostream& log(Level level = Level::INFO) {
    // protect the Logger::log function
    std::lock_guard<std::mutex> lock(mutex);

    std::ostream& out = (*os);

    // TODO: if level < withLevel, returns os -> /dev/null

    auto now = std::chrono::system_clock::now().time_since_epoch();
    auto unixMillis = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();

    if (withStdout) {
      switch (level) {
        case Level::INFO:
          out << "[INFO]: ";
          break;
        case Level::DEBUG:
          out << "[DEBUG]: ";
          break;
        default:
          break;
      }
    }

    out << unixMillis << ',';

    return out;
  }
};

#endif
