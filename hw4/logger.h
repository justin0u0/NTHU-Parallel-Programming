#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <chrono>
#include <fstream>
#include <iostream>

class Logger {
private:
	std::ostream* os;
	std::ofstream* ofs;

	bool withStdout;
public:
	enum class Level {
		INFO,
		DEBUG
	};

	Logger() {
		os = new std::ostream(std::cout.rdbuf());

		withStdout = true;
	}

	Logger(const char* filename) {
		ofs = new std::ofstream(filename);

		os = new std::ostream(ofs->rdbuf());

		withStdout = false;
	}

	Logger(const std::string& filename) : Logger(filename.c_str()) {}

	~Logger() {
		delete os;
		delete ofs;
	}

	std::ostream& With(Level level) {
		std::ostream& out = (*os);

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
