#pragma once
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>
#include <algorithm>
#include <cctype>
#include <stdexcept>
#include "Basics.h"
#include "BranchPredictor.h"
#include "ExecutionUnit.h"
#include "LoadStoreQueue.h"

class Processor {
public:
    //global program-counter.
    int pc;
    
    //global clock cycle count.
    int clock_cycle;


    std::vector<Instruction> inst_memory;

    //actual memory in hardware.
    std::vector<int> ARF;
    
    std::vector<int> Memory;
    bool exception = false;

    std::vector<ExecutionUnit> units;
    LoadStoreQueue* lsq;
    BranchPredictor bp;

    std::vector<int> RAT;
    std::vector<ROBEntry> ROB;
    int rob_head = 0;
    int rob_tail = 0;
    int rob_count = 0;
    int next_tag = 0;

    bool fetched_valid = false;
    Instruction fetched_inst;
    int fetched_predicted_pc = -1;

    bool halted = false;
    ProcessorConfig cfg;

    Processor(ProcessorConfig& config);
    ~Processor();

    void loadProgram(const std::string& filename);
    void flush();
    void broadcastOnCDB();
    void stageFetch();
    void stageDecode();
    void stageExecuteAndBroadcast();
    void stageCommit();
    bool step();
    void dumpArchitecturalState();

private:
    static std::string trim(const std::string &s);
    static std::string cleanLine(const std::string &s);
    static int regNum(std::string s);
    static std::vector<std::string> splitOperands(const std::string &s);

    Instruction parseInstruction(const std::string &line, int curr_pc);
    static void parseMemArg(const std::string &s, int &imm, int &base);
    ExecutionUnit& getUnitRef(UnitType type);
    UnitType getUnit(OpCode op);
    int allocROB(const Instruction &ins);
    int indexFromTag(int tag);
    static bool writesReg(OpCode op);
    void fillOperands(RSEntry &entry, const Instruction &ins);
    void fillOperands(LSQEntry &entry, const Instruction &ins);
    void readSource(int reg, bool &ready, int &val, int &tag);
    void writeResult(const UnitResult &out);
    void popROB();
    bool pipelineEmpty();
};
