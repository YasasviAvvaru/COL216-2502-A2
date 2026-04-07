#include "ExecutionUnit.h"

ExecutionUnit::ExecutionUnit() {
}

ExecutionUnit::ExecutionUnit(UnitType type, int lat, int sz) {
    name = type;
    latency = lat;
    rs_size = sz;
    rs.resize(sz);
}

bool ExecutionUnit::hasFreeRS() const {
    for (auto &entry : rs) {
        if (!entry.busy) {
            return true;
        }
    }
    return false;
}

bool ExecutionUnit::addEntry(const RSEntry &entry) {
    for (auto &slot : rs) {
        if (!slot.busy) {
            slot = entry;
            return true;
        }
    }
    return false;
}

void ExecutionUnit::capture(int tag, int val) {
    for (auto &entry : rs) {
        if (!entry.busy) {
            continue;
        }

        if (!entry.ready1 && entry.q1 == tag) {
            entry.ready1 = true;
            entry.q1 = -1;
            entry.v1 = val;
        }

        if (!entry.ready2 && entry.q2 == tag) {
            entry.ready2 = true;
            entry.q2 = -1;
            entry.v2 = val;
        }
    }
}

void ExecutionUnit::executeCycle() {
    finished.clear();
    has_result = false;
    has_exception = false;

    int pick = -1;
    for (int i = 0; i < (int)rs.size(); i++) {
        if (!rs[i].busy) {
            continue;
        }
        if (!rs[i].ready1 || !rs[i].ready2) {
            continue;
        }

        if (pick == -1 || rs[i].order_pc < rs[pick].order_pc) {
            pick = i;
        }
    }

    if (pick != -1) {
        PipelineJob job;
        job.tag = rs[pick].dest_tag;
        job.order_pc = rs[pick].order_pc;
        job.op1 = rs[pick].v1;
        job.op2 = rs[pick].v2;
        job.imm = rs[pick].imm;
        job.left = latency;
        job.op = rs[pick].op;

        pipe.push_back(job);
        rs[pick].busy = false;
    }

    for (auto &job : pipe) {
        job.left--;
    }

    std::vector<PipelineJob> next_pipe;
    for (auto &job : pipe) {
        if (job.left == 0) {
            UnitResult out = solve(job);
            finished.push_back(out);

            if (out.has_exception) {
                has_exception = true;
            }
        } else {
            next_pipe.push_back(job);
        }
    }

    pipe = next_pipe;
    has_result = !finished.empty();
}

UnitResult ExecutionUnit::solve(const PipelineJob &job) {
    UnitResult out;
    out.tag = job.tag;

    int a = job.op1;
    int b = job.op2;
    long long ans = 0;

    switch (job.op) {
        case OpCode::ADD:
            ans = 1LL * a + b;
            break;
        case OpCode::SUB:
            ans = 1LL * a - b;
            break;
        case OpCode::ADDI:
            ans = 1LL * a + job.imm;
            break;
        case OpCode::MUL:
            ans = 1LL * a * b;
            break;
        case OpCode::DIV:
            if (b == 0) out.has_exception = true;
            else ans = a / b;
            break;
        case OpCode::REM:
            if (b == 0) out.has_exception = true;
            else ans = a % b;
            break;
        case OpCode::SLT:
            ans = (a < b ? 1 : 0);
            break;
        case OpCode::SLTI:
            ans = (a < job.imm ? 1 : 0);
            break;
        case OpCode::AND:
            ans = (a & b);
            break;
        case OpCode::OR:
            ans = (a | b);
            break;
        case OpCode::XOR:
            ans = (a ^ b);
            break;
        case OpCode::ANDI:
            ans = (a & job.imm);
            break;
        case OpCode::ORI:
            ans = (a | job.imm);
            break;
        case OpCode::XORI:
            ans = (a ^ job.imm);
            break;
        case OpCode::BEQ:
            out.branch_taken = (a == b);
            break;
        case OpCode::BNE:
            out.branch_taken = (a != b);
            break;
        case OpCode::BLT:
            out.branch_taken = (a < b);
            break;
        case OpCode::BLE:
            out.branch_taken = (a <= b);
            break;
        default:
            break;
    }

    if (!out.has_exception) {
        bool check_overflow =
            job.op == OpCode::ADD || job.op == OpCode::SUB ||
            job.op == OpCode::ADDI || job.op == OpCode::MUL ||
            job.op == OpCode::DIV || job.op == OpCode::REM;

        if (check_overflow) {
            if (ans > 2147483647LL || ans < -2147483648LL) {
                out.has_exception = true;
            }
        }

        out.value = (int)ans;
    }

    return out;
}
