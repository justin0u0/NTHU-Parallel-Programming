#include "logger.h"

int main() {
  Logger* logger = new Logger;

  logger->log(Logger::Level::INFO) << "hello" << std::endl;

  Logger* fileLogger = new Logger("./tests/logger_test.log");

  filelogger->log() << "hello" << std::endl;

  delete logger;
  delete fileLogger;

  return 0;
}
