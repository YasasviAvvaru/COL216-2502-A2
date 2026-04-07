#pragma once
#include <vector>
#include "Basics.h"

struct UnitResult {
    int tag = -1;
    int value = 0;
    int store_data = 0;
    bool has_exception = false;
    int actual_next_pc = -1;
    bool branch_taken = false;
};

struct PipelineJob {
    int tag = -1;
    int order_pc = -1;
    int op1 = 0;
    int op2 = 0;
    int imm = 0;
    int left = 0;
    OpCode op = OpCode::ADD;
};

class ExecutionUnit {
public:
    UnitType name = UnitType::ADDER;
    int latency = 1;
    int rs_size = 0;

    bool has_result = false;
    bool has_exception = false;
    std::vector<RSEntry> rs;
    std::vector<PipelineJob> pipe;
    std::vector<UnitResult> finished;

    ExecutionUnit();
    ExecutionUnit(UnitType t, int lat, int sz);

    bool hasFreeRS() const;
    bool addEntry(const RSEntry &e);
    void capture(int tag, int val);
    void executeCycle();

private:
    UnitResult solve(const PipelineJob &job);
};
