#include "logger.h"

int main() {
	Logger* logger = new Logger;

	logger->With(Logger::Level::INFO) << "hello" << std::endl;

	Logger* fileLogger = new Logger("./tests/logger_test.log");

	fileLogger->With(Logger::Level::INFO) << "hello" << std::endl;

	delete logger;
	delete fileLogger;

	return 0;
}
