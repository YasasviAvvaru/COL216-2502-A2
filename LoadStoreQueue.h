#pragma once
#include <vector>
#include "Basics.h"
#include "ExecutionUnit.h"

struct LSQEntry {
    bool busy = false;
    int order_pc = -1;
    int dest_tag = -1;
    OpCode op = OpCode::LW;
    int imm = 0;

    bool ready1 = false;
    bool ready2 = false;
    int v1 = 0;
    int v2 = 0;
    int q1 = -1;
    int q2 = -1;
};

struct LSQJob {
    int tag = -1;
    OpCode op = OpCode::LW;
    int base = 0;
    int imm = 0;
    int store_data = 0;
    int left = 0;
};

class LoadStoreQueue {
public:
    int latency = 1;
    int rs_size = 0;

    bool has_result = false;
    bool has_exception = false;

    std::vector<LSQEntry> entries;
    std::vector<LSQJob> pipe;
    std::vector<UnitResult> finished;

    LoadStoreQueue();
    LoadStoreQueue(int lat, int sz);

    bool hasFreeRS() const;
    bool addEntry(const LSQEntry &entry);
    void capture(int tag, int val);
    void executeCycle(std::vector<int> &memory);
};
