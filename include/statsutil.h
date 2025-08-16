#ifndef __defined_stats_util_h
#define __defined_stats_util_h

#include "sys/types.h"
#include "sys/sysinfo.h"
#include "iostream"
#include "stdlib.h"
#include "stdio.h"
#include "string.h"

long long virtualMem(){

    struct sysinfo memInfo;

    sysinfo(&memInfo);
    long long totalVirtualMem = memInfo.totalram;
    //Add other values in next statement to avoid int overflow on right hand side...
    totalVirtualMem += memInfo.totalswap;
    totalVirtualMem *= memInfo.mem_unit;
    
    return totalVirtualMem;
}

long long virtualMemUsed(){

    struct sysinfo memInfo;

    sysinfo(&memInfo);
    long long virtualMemUsed = memInfo.totalram - memInfo.freeram;
    //Add other values in next statement to avoid int overflow on right hand side...
    virtualMemUsed += memInfo.totalswap - memInfo.freeswap;
    virtualMemUsed *= memInfo.mem_unit;

    return virtualMemUsed;
}

int parseLine(char* line){
    // This assumes that a digit will be found and the line ends in " Kb".
    int i = strlen(line);
    const char* p = line;
    while (*p <'0' || *p > '9') p++;
    line[i-3] = '\0';
    i = atoi(p);
    return i;
}

long long virtualMemUsedByProcess(){
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmSize:", 7) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result*1024;
}

long long physMem(){

    struct sysinfo memInfo;

    sysinfo(&memInfo);
    long long totalPhysMem = memInfo.totalram;
    //Multiply in next statement to avoid int overflow on right hand side...
    totalPhysMem *= memInfo.mem_unit;
    
    return totalPhysMem;
}

long long physMemUsed(){

    struct sysinfo memInfo;

    sysinfo(&memInfo);
    long long physMemUsed = memInfo.totalram - memInfo.freeram;
    //Multiply in next statement to avoid int overflow on right hand side...
    physMemUsed *= memInfo.mem_unit;
    
    return physMemUsed;
}

int physMemUsedByProcess(){
    FILE* file = fopen("/proc/self/status", "r");
    int result = -1;
    char line[128];

    while (fgets(line, 128, file) != NULL){
        if (strncmp(line, "VmRSS:", 6) == 0){
            result = parseLine(line);
            break;
        }
    }
    fclose(file);
    return result*1024;
}


int printAllMemStats() {

    long long mem = virtualMem();
    std::cout << "Virtual Memory (bytes): " << mem << std::endl;
    std::cout << "Virtual Memory (GB): " << mem*1e-9 << std::endl;

    mem = virtualMemUsed();
    std::cout << "Virtual Memory Used (bytes): " << mem << std::endl;
    std::cout << "Virtual Memory Used (GB): " << mem*1e-9 << std::endl;

    mem = virtualMemUsedByProcess();
    std::cout << "Virtual Memory Used By Process (bytes): " << mem << std::endl;
    std::cout << "Virtual Memory Used By Process (GB): " << mem*1e-9 << std::endl;

    mem = physMem();
    std::cout << "Physical Memory (bytes): " << mem << std::endl;
    std::cout << "Physical Memory (GB): " << mem*1e-9 << std::endl;

    mem = physMemUsed();
    std::cout << "Physical Memory Used(bytes): " << mem << std::endl;
    std::cout << "Physical Memory Used (GB): " << mem*1e-9 << std::endl;

    mem = physMemUsedByProcess();
    std::cout << "Physical Memory Used By Process (bytes): " << mem << std::endl;
    std::cout << "Physical Memory Used By Process (GB): " << mem*1e-9 << std::endl;

    return 0;
}

#endif
