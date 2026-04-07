#include "ExecutionUnit.h"

ExecutionUnit::ExecutionUnit() {
}

ExecutionUnit::ExecutionUnit(UnitType t, int lat, int sz) {
    name = t;
    latency = lat;
    rs_size = sz;
    rs.resize(sz);
}

bool ExecutionUnit::hasFreeRS() const {
    for (auto &x : rs) {
        if (!x.busy) {
            return true;
        }
    }
    return false;
}

bool ExecutionUnit::addEntry(const RSEntry &e) {
    for (auto &x : rs) {
        if (!x.busy) {
            x = e;
            return true;
        }
    }
    return false;
}

void ExecutionUnit::capture(int tag, int val) {
    for (auto &x : rs) {
        if (!x.busy) {
            continue;
        }

        if (!x.ready1 && x.q1 == tag) {
            x.ready1 = true;
            x.q1 = -1;
            x.v1 = val;
        }

        if (!x.ready2 && x.q2 == tag) {
            x.ready2 = true;
            x.q2 = -1;
            x.v2 = val;
        }
    }
}

void ExecutionUnit::executeCycle() {
    finished.clear();
    has_result = false;
    has_exception = false;

    int best = -1;
    for (int i = 0; i < (int)rs.size(); i++) {
        if (!rs[i].busy) {
            continue;
        }
        if (!rs[i].ready1 || !rs[i].ready2) {
            continue;
        }

        if (best == -1 || rs[i].order_pc < rs[best].order_pc) {
            best = i;
        }
    }

    if (best != -1) {
        PipelineJob cur;
        cur.tag = rs[best].dest_tag;
        cur.order_pc = rs[best].order_pc;
        cur.op1 = rs[best].v1;
        cur.op2 = rs[best].v2;
        cur.imm = rs[best].imm;
        cur.left = latency;
        cur.op = rs[best].op;

        pipe.push_back(cur);
        rs[best].busy = false;
    }

    for (auto &x : pipe) {
        x.left--;
    }

    std::vector<PipelineJob> nxt;
    for (auto &x : pipe) {
        if (x.left == 0) {
            UnitResult out = solve(x);
            finished.push_back(out);

            if (out.has_exception) {
                has_exception = true;
            }
        } else {
            nxt.push_back(x);
        }
    }

    pipe = nxt;
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
        bool checkOverflow =
            job.op == OpCode::ADD || job.op == OpCode::SUB ||
            job.op == OpCode::ADDI || job.op == OpCode::MUL ||
            job.op == OpCode::DIV || job.op == OpCode::REM;

        if (checkOverflow) {
            if (ans > 2147483647LL || ans < -2147483648LL) {
                out.has_exception = true;
            }
        }

        out.value = (int)ans;
    }

    return out;
}
