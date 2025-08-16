# include <iostream>
# include <fstream>
# include <dai/logger.h>

namespace dai {


    LibLogger::LibLogger(const std::string& filename, LogLevel minLevel = LogLevel::INFO){

        LibLogger::minLogLevel = minLevel;
        logFile.open(filename, std::ios::app);
        if(!logFile.is_open()){
            std::cerr << "Error opening log file: " << filename << std::endl;
        }
    }

    LibLogger::~LibLogger(){
        logFile.close();
    }


    void LibLogger::log(LogLevel level, const std::string& message) {
        
        // Skip logs below the minimum log level
        if(static_cast<int>(level) < static_cast<int>(minLogLevel)){
            return; 
        }

        const char* levelStrings[] = {"DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"};
        std::string logMessage = "[" + std::string(levelStrings[static_cast<int>(level)]) + "] " + message;

        if(logFile.is_open()){
            logFile << logMessage << std::endl;
        }
    }

}


