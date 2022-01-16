#include "logger.h"

int main() {
  Logger* logger = new Logger;

  logger->Log(Logger::Level::INFO) << "hello" << std::endl;

  Logger* fileLogger = new Logger("./tests/logger_test.log");

  fileLogger->Log() << "hello" << std::endl;

  delete logger;
  delete fileLogger;

  return 0;
}
