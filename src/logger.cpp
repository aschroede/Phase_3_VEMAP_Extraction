# include <iostream>
# include <fstream>
# include <dai/logger.h>
# include <filesystem>

namespace fs = std::filesystem;

namespace dai {


    LibLogger::LibLogger(const std::string& filepath, LogLevel minLevel = LogLevel::INFO){

        LibLogger::minLogLevel = minLevel;

        // Create the file and directory if needed
        try
        {
            fs::path dir = fs::path(filepath).parent_path();
            if (!dir.empty())
            {
                fs::create_directories(dir);
            }

            std::ofstream outfile(filepath, std::ios::trunc); // trunc overwrites
            if (outfile)
            {
                outfile << "This file was freshly created or overwritten.\n";
                std::cout << "File successfully created/overwritten at: " << filepath << std::endl;
            } else {
                std::cerr << "Error creating/overwriting file at: " << filepath << std::endl;
            }
        }
        catch (const fs::filesystem_error& e)
        {
            std::cerr << "Filesystem error: " << e.what() << std::endl;
        }

        logFile.open(filepath, std::ios::app);
        if(!logFile.is_open()){
            std::cerr << "Error opening log file: " << filepath << std::endl;
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


