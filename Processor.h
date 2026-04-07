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
    int pc;
    int clock_cycle;

    std::vector<Instruction> inst_memory;

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
    bool suppress_fetch_this_cycle = false;
    std::vector<int> decode_arf_snapshot;
    std::vector<int> decode_rat_snapshot;
    std::vector<ROBEntry> decode_rob_snapshot;
    int decode_rob_count_snapshot = 0;
    bool decode_lsq_free_snapshot = false;
    std::vector<bool> decode_unit_free_snapshot;

    bool halted = false;
    ProcessorConfig cfg;

    Processor(ProcessorConfig &config);
    ~Processor();

    void loadProgram(const std::string &filename);
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

    Instruction parseInstruction(const std::string &line, int curPc);
    static void parseMemArg(const std::string &s, int &imm, int &base);
    ExecutionUnit &getUnitRef(UnitType type);
    UnitType getUnit(OpCode op);
    int allocROB(const Instruction &ins);
    int indexFromTag(int tag);
    static bool writesReg(OpCode op);
    void fillOperands(RSEntry &entry, const Instruction &ins);
    void fillOperands(LSQEntry &entry, const Instruction &ins);
    void readSource(int reg, bool &ready, int &val, int &tag);
    int indexFromTagSnapshot(int tag) const;
    void writeResult(const UnitResult &out);
    void popROB();
    bool pipelineEmpty();
};
