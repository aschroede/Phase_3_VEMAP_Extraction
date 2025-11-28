#ifndef __defined_libdai_logger_h
#define __defined_libdai_logger_h

# include <iostream>
# include <fstream>

namespace dai {

    constexpr std::string_view LOG_LEVEL_STRINGS[] = {
        "DEBUG",
        "INFO",
        "WARNING",
        "ERROR",
        "CRITICAL"
    };

    enum class LogLevel {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        CRITICAL
    };

    class LibLogger {

        private:
            std::ofstream logFile;
            LogLevel minLogLevel;

        public:
            
            LibLogger();

            LibLogger(const std::string& filepath, LogLevel minLevel);

            ~LibLogger();

            void log(LogLevel level, const std::string& message);
    };
}
#endif