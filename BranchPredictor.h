#pragma once
#include "Basics.h"
#include <vector>

class BranchPredictor {
public:
    int total_branches = 0;
    int correct_predictions = 0;
    std::vector<int> state;

    int predict(int pc, int imm, OpCode op);
    void update(int pc, int actual_target, bool taken, bool was_correct);
    bool isBranch(OpCode op) const;
};
