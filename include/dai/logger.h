#ifndef __defined_libdai_logger_h
#define __defined_libdai_logger_h

# include <iostream>
# include <fstream>

namespace dai {

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

            LibLogger(const std::string& filename, LogLevel minLevel);

            ~LibLogger();

            void log(LogLevel level, const std::string& message);
    };
}
#endif