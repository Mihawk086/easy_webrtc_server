#include "logger.h"

/* Class variables. */

const int64_t Logger::pid{static_cast<int64_t>(0)};
LogChannel* Logger::channel{nullptr};
char Logger::buffer[Logger::bufferSize];

/* Class methods. */

void Logger::ClassInit(LogChannel* channel) {
  Logger::channel = channel;
  MS_TRACE();
}
