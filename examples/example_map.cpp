#include <dai/factorgraph.h>
#include "cxxopts.hpp"
#include <dai/map.h>
#include <chrono>
#include <ctime>
#include <filesystem>
#include "example_map.h"
#include <dai/logger.h>



using namespace std;
using namespace dai;


// comment for production mode, uncomment for debug messages
#define DEBUGMODE

#ifdef DEBUGMODE
	#define DEBUG(a) a;
#else
	#define DEBUG(a) ;
#endif	

std::string testdir = "TestResults";
std::string inputfile = "./alarm.fg";
std::string outputfile = "./results";
LogLevel logLevel = LogLevel::INFO;
std::vector<unsigned int> hypothesisVars;
std::vector<unsigned int> evidenceVars;
std::vector<unsigned int> evidenceValues;

bool jtMapComputation = false;
bool veMapComputation = false;
bool veComputation = false;

cxxopts::ParseResult parse(int argc, char* argv[])
{
    const char *shortdes = "MAP, MFE, and Annealed MAP experimental simulation";
    try
    {
        cxxopts::Options options(argv[0], shortdes);
        options.add_options()
            ("i,input", "factor graph to run simulations on", cxxopts::value<std::string>())
            ("o,output", "output file for simulation results", cxxopts::value<std::string>())
            ("l, log-level", "verbosity of logging [DEBUG, INFO, WARNING, ERROR, CRITICAL]", cxxopts::value<std::string>())
            ("H,hypothesis-variables", "hypothesis variables", cxxopts::value<std::vector<unsigned int>>())
            ("E,evidence-variables", "evidence variables", cxxopts::value<std::vector<unsigned int>>())
            ("e,evidence-values", "values of the evidence variables", cxxopts::value<std::vector<unsigned int>>())
            ("J,jtmap", "run exact MAP computation")
            ("M,vemap", "run exact MAP using variable elimination")
            ("V,ve", "run a variable elimination query")
        ;

        if (argc == 1)
        {
          std::cout << shortdes << std::endl;
          exit(0);
        }
    
        auto result = options.parse(argc, argv);

        if (result.count("help"))
        {
          std::cout << options.help({"", "Group"}) << std::endl;
          exit(0);
        }

        if (result.count("jtmap"))
        {
            jtMapComputation = true;  
            DEBUG(std::cout << "Exact computation using MAP" << std::endl)
        }

        if (result.count("vemap")){
            veMapComputation = true;
            DEBUG(std::cout << "Exact computation using VE MAP" << std::endl)
        }

        if (result.count("ve")){
            veComputation = true;
            DEBUG(std::cout << "Exact computation using VE" << std::endl)
        }

        if (result.count("input"))
        {
            inputfile = result["input"].as<std::string>();
            DEBUG(std::cout << "Input file: " << inputfile << std::endl)
        }

        if (result.count("output"))
        {
            outputfile = result["output"].as<std::string>();
            DEBUG(std::cout << "Output file: " << outputfile << std::endl)
        }

        if (result.count("log-level"))
        {
            std::string level = result["log-level"].as<std::string>();

            if(level == "DEBUG")
                logLevel = LogLevel::DEBUG;
            else if (level == "INFO")
                logLevel = LogLevel::INFO;
            else if (level == "WARNING")
                logLevel = LogLevel::WARNING;
            else if (level == "ERROR")
                logLevel = LogLevel::ERROR;
            else if (level == "CRITICAL")
                logLevel = LogLevel::CRITICAL;
        }

        if (result.count("hypothesis-variables"))
        {  
            hypothesisVars = result["hypothesis-variables"].as<std::vector<unsigned int>>();
            DEBUG(
                std::cout << "Hypothesis variables: ";
                for (auto i = hypothesisVars.begin(); i != hypothesisVars.end(); ++i) std::cout << *i << ' ';
                std::cout << std::endl;
                 )
        }

        if (result.count("evidence-variables"))
        {  
            evidenceVars = result["evidence-variables"].as<std::vector<unsigned int>>();
            DEBUG(
                std::cout << "Evidence variables: ";
                for (auto i = evidenceVars.begin(); i != evidenceVars.end(); ++i) std::cout << *i << ' ';
                std::cout << std::endl;
                 )
        }

        if (result.count("evidence-values"))
        {  
            evidenceValues = result["evidence-values"].as<std::vector<unsigned int>>();
            DEBUG(
                std::cout << "Evidence values: ";
                for (auto i = evidenceValues.begin(); i != evidenceValues.end(); ++i) std::cout << *i << ' ';
                std::cout << std::endl;
                 )
        }

        return result;
    } 
    catch (const cxxopts::OptionException& e)
    {
        std::cout << "error parsing options: " << e.what() << std::endl;
        exit(1);
    }
}

void VEMap(dai::FactorGraph &fg, LibLogger &logger)
{
    logger.log(LogLevel::INFO, "\n[MAP] Computing MAP with Variable Elimination ");

    // Start clock
    auto start = std::chrono::steady_clock::now();

    // Perform VE Map
    dai::Factor MAP = get_map_ve(fg, hypothesisVars, evidenceVars, evidenceValues, false, logger);

    // Stop clock
    auto end = std::chrono::steady_clock::now();

    // Format instantiation data
    string instantiation = "";
    for (const auto &myMap : MAP.i())
    {
        for (const auto &entry : myMap)
        {
            instantiation += std::to_string(entry.second) + " ";
        }
    }

    logger.log(LogLevel::INFO, "[MAP] Total Time: " + std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()) + " ns");
    logger.log(LogLevel::INFO, "[MAP] Instantiation: " + instantiation + " has probability " + probToString(MAP.p()));
    
}

void JTMap(dai::FactorGraph &fg, LibLogger &logger)
{
    logger.log(LogLevel::INFO, "\n[MAP] Computing MAP with Junction Tree: ");

    // Perform VE Map
    std::vector<unsigned long int> MAP = get_map_jt(fg, hypothesisVars, evidenceVars, evidenceValues, false, logger);

}

void VE(dai::FactorGraph &fg, LibLogger &logger)
{
    logger.log(LogLevel::INFO, "Running VE Algorithm: ");

    // Perform VE Map
    dai::Factor prob_dist = variableElimination(fg, hypothesisVars, evidenceVars, evidenceValues, logger);
    extractMax(prob_dist, logger);

}

int main( int argc, char *argv[] ) {
    namespace fs = std::filesystem;
    
    // Get arguments
    auto result = parse(argc, argv);
    auto arguments = result.arguments();

    // Get time
    time_t now = time(0);

    // Read graph data
   	dai::FactorGraph fg;
   	fg.ReadFromFile(inputfile.c_str());

    // File output stream
	std::ofstream ofs;
    std::string filepath = testdir + "/" + outputfile;

    // Create directory for storing results
    if(!fs::exists(testdir)){

        if(!fs::create_directory(testdir)){
            std::cerr << "Error creating direcotyr: " << testdir << std::endl;
        }
    }
	ofs.open (filepath.c_str(), std::ofstream::out | std::ofstream::app);

    
    dai::LibLogger logger = dai::LibLogger(testdir + "/" + outputfile, logLevel);
    logger.log(LogLevel::DEBUG, "This is a test");
    
    // Write command to output file for reference
    std:: string command = "command: ";
    for (int i = 0; i < argc; i++)
        command += std::string(argv[i]) + ' ';
    logger.log(LogLevel::INFO, command);

    // Record relevent parameters in log file
    logger.log(LogLevel::INFO, inputfile + " simulation results " + ctime(&now));
    
    std::ostringstream oss;
    logger.log(LogLevel::INFO, "hypothesis vars " + vecToString(hypothesisVars));
    logger.log(LogLevel::INFO, "evidence vars " + vecToString(evidenceVars) + " values " + vecToString(evidenceValues));


    if (jtMapComputation)
    {
        JTMap(fg, logger);
    }

    if (veMapComputation){
        VEMap(fg, logger);
    }

    if(veComputation){
        VE(fg, logger);
    }

    ofs << std::endl;
	ofs.close();
}

