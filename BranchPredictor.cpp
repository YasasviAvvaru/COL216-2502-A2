#include "BranchPredictor.h"

int BranchPredictor::predict(int pc, int imm, OpCode op) {
    if (op == OpCode::J) {
        return pc + imm;
    }

    if (!isBranch(op)) {
        return pc + 1;
    }

    if (pc >= (int)state.size()) {
        state.resize(pc + 1, 0);
    }

    // 0,1 -> taken ; 2,3 -> not taken
    if (state[pc] <= 1) {
        return pc + imm;
    }

    return pc + 1;
}

void BranchPredictor::update(int pc, int actual_target, bool taken, bool was_correct) {
    (void)actual_target;
    if (pc < 0) {
        return;
    }
    
    if (pc >= (int)state.size()) {
        state.resize(pc + 1, 0);
    }
    
    total_branches++;
    if (was_correct) {
        correct_predictions++;
    }

    int &curr = state[pc];
    if (taken) {
        if (curr > 0) {
            curr--;
        }
    } else if (curr < 3) {
        curr++;
    }
}

//checks whether the op is a branch instr
bool BranchPredictor::isBranch(OpCode op) const {
    return op == OpCode::BEQ || op == OpCode::BNE ||
           op == OpCode::BLT || op == OpCode::BLE;
}
