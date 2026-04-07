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

    // tiny 2-bit thing
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

    int &cur = state[pc];
    if (taken) {
        if (cur > 0) {
            cur--;
        }
    } else if (cur < 3) {
        cur++;
    }
}

bool BranchPredictor::isBranch(OpCode op) const {
    return op == OpCode::BEQ || op == OpCode::BNE ||
           op == OpCode::BLT || op == OpCode::BLE;
}
