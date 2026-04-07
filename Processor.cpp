#include "Processor.h"

Processor::Processor(ProcessorConfig &config) {
    cfg = config;
    pc = 0;
    clock_cycle = 0;

    ARF.resize(config.num_regs, 0);
    Memory.resize(config.mem_size, 0);
    RAT.resize(config.num_regs, -1);
    ROB.resize(config.rob_size);

    units.push_back(ExecutionUnit(UnitType::ADDER, config.add_lat, config.adder_rs_size));
    units.push_back(ExecutionUnit(UnitType::MULTIPLIER, config.mul_lat, config.mult_rs_size));
    units.push_back(ExecutionUnit(UnitType::DIVIDER, config.div_lat, config.div_rs_size));
    units.push_back(ExecutionUnit(UnitType::BRANCH, config.add_lat, config.br_rs_size));
    units.push_back(ExecutionUnit(UnitType::LOGIC, config.logic_lat, config.logic_rs_size));

    lsq = new LoadStoreQueue(config.mem_lat, config.lsq_rs_size);
}

Processor::~Processor() {
    delete lsq;
}

void Processor::loadProgram(const std::string &filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open file");
    }

    inst_memory.clear();
    std::fill(Memory.begin(), Memory.end(), 0);

    std::string line;
    int curPc = 0;
    int memPtr = 0;

    while (std::getline(file, line)) {
        line = cleanLine(line);
        if (line.empty()) {
            continue;
        }

        if (line[0] == '.') {
            int pos = (int)line.find(':');
            if (pos == -1) {
                continue;
            }

            std::string rest = trim(line.substr(pos + 1));
            std::stringstream ss(rest);
            int x;

            while (ss >> x) {
                if (memPtr < (int)Memory.size()) {
                    Memory[memPtr] = x;
                }
                memPtr++;
            }
            continue;
        }

        if (line.back() == ':') {
            continue;
        }

        size_t pos = line.find(':');
        if (pos != std::string::npos) {
            line = trim(line.substr(pos + 1));
            if (line.empty()) {
                continue;
            }
        }

        inst_memory.push_back(parseInstruction(line, curPc));
        curPc++;
    }
}

void Processor::flush() {
    fetched_valid = false;

    for (auto &unit : units) {
        for (auto &entry : unit.rs) {
            entry = RSEntry();
        }

        unit.pipe.clear();
        unit.finished.clear();
        unit.has_result = false;
        unit.has_exception = false;
    }

    lsq->entries.clear();
    lsq->pipe.clear();
    lsq->finished.clear();
    lsq->has_result = false;
    lsq->has_exception = false;

    for (auto &entry : ROB) {
        entry = ROBEntry();
    }

    std::fill(RAT.begin(), RAT.end(), -1);

    rob_head = 0;
    rob_tail = 0;
    rob_count = 0;
}

void Processor::broadcastOnCDB() {
    for (auto &u : units) {
        for (auto &out : u.finished) {
            writeResult(out);
        }
    }

    for (auto &out : lsq->finished) {
        writeResult(out);
    }
}

void Processor::stageFetch() {
    if (halted || exception) {
        return;
    }
    if (suppress_fetch_this_cycle) {
        return;
    }
    if (fetched_valid) {
        return;
    }
    if (pc < 0 || pc >= (int)inst_memory.size()) {
        return;
    }

    fetched_inst = inst_memory[pc];
    fetched_predicted_pc = bp.predict(pc, fetched_inst.imm, fetched_inst.op);
    fetched_valid = true;
    pc = fetched_predicted_pc;
}

void Processor::stageDecode() {
    if (!fetched_valid || halted || exception) {
        return;
    }

    if (decode_rob_count_snapshot == cfg.rob_size) {
        return;
    }

    Instruction ins = fetched_inst;
    UnitType unitType = getUnit(ins.op);

    if (ins.op != OpCode::J) {
        if (unitType == UnitType::LOADSTORE) {
            if (!decode_lsq_free_snapshot) {
                return;
            }
        } else {
            int unitIdx = -1;
            for (int i = 0; i < (int)units.size(); i++) {
                if (units[i].name == unitType) {
                    unitIdx = i;
                    break;
                }
            }
            if (unitIdx == -1 || !decode_unit_free_snapshot[unitIdx]) {
                return;
            }
        }
    }

    int tag = allocROB(ins);
    if (tag == -1) {
        return;
    }

    if (ins.op == OpCode::J) {
        int idx = indexFromTag(tag);
        ROB[idx].ready = true;
        ROB[idx].actual_next_pc = ins.pc + ins.imm;

        fetched_valid = false;
        return;
    }

    if (unitType == UnitType::LOADSTORE) {
        LSQEntry e;
        fillOperands(e, ins);

        e.busy = true;
        e.order_pc = ins.pc;
        e.dest_tag = tag;
        e.op = ins.op;
        e.imm = ins.imm;

        lsq->addEntry(e);
    } else {
        RSEntry e;
        fillOperands(e, ins);

        e.busy = true;
        e.order_pc = ins.pc;
        e.dest_tag = tag;
        e.op = ins.op;
        e.imm = ins.imm;

        getUnitRef(unitType).addEntry(e);
    }

    if (writesReg(ins.op) && ins.dest != 0) {
        RAT[ins.dest] = tag;
    }

    fetched_valid = false;
}

void Processor::stageExecuteAndBroadcast() {
    for (auto &u : units) {
        u.executeCycle();
    }

    lsq->executeCycle(Memory, ROB);
}

void Processor::stageCommit() {
    if (rob_count == 0) {
        return;
    }

    ROBEntry head = ROB[rob_head];
    if (!head.busy || !head.ready) {
        return;
    }

    if (head.has_exception) {
        pc = head.pc;
        exception = true;
        suppress_fetch_this_cycle = true;
        flush();
        halted = true;
        return;
    }

    if (head.is_store) {
        Memory[head.store_addr] = head.store_data;
    } else if (head.writes_reg && head.dest_reg != 0) {
        ARF[head.dest_reg] = head.value;

        if (RAT[head.dest_reg] == head.tag) {
            RAT[head.dest_reg] = -1;
        }
    }

    bool isCondBranch = bp.isBranch(head.op);
    bool mispred = false;

    if (head.op == OpCode::J || isCondBranch) {
        mispred = (head.predicted_pc != head.actual_next_pc);
    }

    popROB();

    if (isCondBranch) {
        bp.update(head.pc, head.actual_next_pc, head.branch_taken, !mispred);
    }

    if (mispred) {
        flush();
        pc = head.actual_next_pc;
        suppress_fetch_this_cycle = true;
    }
}

bool Processor::step() {
    if (halted) {
        return false;
    }

    decode_arf_snapshot = ARF;
    decode_rat_snapshot = RAT;
    decode_rob_snapshot = ROB;
    suppress_fetch_this_cycle = false;
    decode_rob_count_snapshot = rob_count;
    decode_lsq_free_snapshot = lsq->hasFreeRS();
    decode_unit_free_snapshot.clear();
    for (const auto &u : units) {
        decode_unit_free_snapshot.push_back(u.hasFreeRS());
    }

    stageCommit();
    stageExecuteAndBroadcast();
    stageDecode();
    broadcastOnCDB();
    stageFetch();

    clock_cycle++;

    if (halted || exception) {
        return false;
    }

    if (pc >= (int)inst_memory.size() && !fetched_valid &&
        rob_count == 0 && pipelineEmpty()) {
        return false;
    }

    return true;
}

void Processor::dumpArchitecturalState() {
    std::cout << "\n=== ARCHITECTURAL STATE (CYCLE " << clock_cycle << ") ===\n";

    for (int i = 0; i < (int)ARF.size(); i++) {
        std::cout << "x" << i << ": " << std::setw(4) << ARF[i] << " | ";
        if ((i + 1) % 8 == 0) {
            std::cout << std::endl;
        }
    }

    if (exception) {
        std::cout << "EXCEPTION raised by instruction " << pc + 1 << std::endl;
    }

    std::cout << "Branch Predictor Stats: "
              << bp.correct_predictions << "/"
              << bp.total_branches << " correct.\n";
}

std::string Processor::trim(const std::string &s) {
    int i = 0;
    int j = (int)s.size() - 1;

    while (i <= j && std::isspace((unsigned char)s[i])) {
        i++;
    }
    while (j >= i && std::isspace((unsigned char)s[j])) {
        j--;
    }

    if (i > j) {
        return "";
    }

    return s.substr(i, j - i + 1);
}

std::string Processor::cleanLine(const std::string &s) {
    std::string t = s;
    size_t pos = t.find('#');

    if (pos != std::string::npos) {
        t = t.substr(0, pos);
    }

    return trim(t);
}

int Processor::regNum(std::string s) {
    s = trim(s);

    if (!s.empty() && s[0] == 'x') {
        return std::stoi(s.substr(1));
    }

    return -1;
}

std::vector<std::string> Processor::splitOperands(const std::string &s) {
    std::vector<std::string> out;
    std::string t = s;

    for (char &c : t) {
        if (c == ',') {
            c = ' ';
        }
    }

    std::stringstream ss(t);
    std::string x;
    while (ss >> x) {
        out.push_back(x);
    }

    return out;
}

Instruction Processor::parseInstruction(const std::string &line, int curPc) {
    std::stringstream ss(line);
    std::string op;
    ss >> op;

    std::string rest;
    std::getline(ss, rest);
    rest = trim(rest);

    std::vector<std::string> args = splitOperands(rest);

    Instruction ins;
    ins.pc = curPc;

    // yeah this is long, but it works
    if (op == "add") {
        ins.op = OpCode::ADD;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "sub") {
        ins.op = OpCode::SUB;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "addi") {
        ins.op = OpCode::ADDI;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "mul") {
        ins.op = OpCode::MUL;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "div") {
        ins.op = OpCode::DIV;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "rem") {
        ins.op = OpCode::REM;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "lw") {
        ins.op = OpCode::LW;
        ins.dest = regNum(args[0]);
        parseMemArg(args[1], ins.imm, ins.src1);
    } else if (op == "sw") {
        ins.op = OpCode::SW;
        ins.src2 = regNum(args[0]);
        parseMemArg(args[1], ins.imm, ins.src1);
    } else if (op == "beq") {
        ins.op = OpCode::BEQ;
        ins.src1 = regNum(args[0]);
        ins.src2 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "bne") {
        ins.op = OpCode::BNE;
        ins.src1 = regNum(args[0]);
        ins.src2 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "blt") {
        ins.op = OpCode::BLT;
        ins.src1 = regNum(args[0]);
        ins.src2 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "ble") {
        ins.op = OpCode::BLE;
        ins.src1 = regNum(args[0]);
        ins.src2 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "j") {
        ins.op = OpCode::J;
        ins.imm = std::stoi(args[0]);
    } else if (op == "slt") {
        ins.op = OpCode::SLT;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "slti") {
        ins.op = OpCode::SLTI;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "and") {
        ins.op = OpCode::AND;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "or") {
        ins.op = OpCode::OR;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "xor") {
        ins.op = OpCode::XOR;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.src2 = regNum(args[2]);
    } else if (op == "andi") {
        ins.op = OpCode::ANDI;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "ori") {
        ins.op = OpCode::ORI;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else if (op == "xori") {
        ins.op = OpCode::XORI;
        ins.dest = regNum(args[0]);
        ins.src1 = regNum(args[1]);
        ins.imm = std::stoi(args[2]);
    } else {
        throw std::runtime_error("bad op");
    }

    return ins;
}

void Processor::parseMemArg(const std::string &s, int &imm, int &base) {
    int p1 = (int)s.find('(');
    int p2 = (int)s.find(')');

    imm = std::stoi(trim(s.substr(0, p1)));
    base = regNum(trim(s.substr(p1 + 1, p2 - p1 - 1)));
}

ExecutionUnit &Processor::getUnitRef(UnitType type) {
    for (auto &u : units) {
        if (u.name == type) {
            return u;
        }
    }

    return units[0];
}

UnitType Processor::getUnit(OpCode op) {
    if (op == OpCode::LW || op == OpCode::SW) {
        return UnitType::LOADSTORE;
    }

    if (op == OpCode::MUL) {
        return UnitType::MULTIPLIER;
    }

    if (op == OpCode::DIV || op == OpCode::REM) {
        return UnitType::DIVIDER;
    }

    if (op == OpCode::BEQ || op == OpCode::BNE ||
        op == OpCode::BLT || op == OpCode::BLE) {
        return UnitType::BRANCH;
    }

    if (op == OpCode::AND || op == OpCode::OR || op == OpCode::XOR ||
        op == OpCode::ANDI || op == OpCode::ORI || op == OpCode::XORI) {
        return UnitType::LOGIC;
    }

    return UnitType::ADDER;
}

int Processor::allocROB(const Instruction &ins) {
    if (rob_count == cfg.rob_size) {
        return -1;
    }

    ROBEntry e;
    e.busy = true;
    e.tag = next_tag++;
    e.pc = ins.pc;
    e.predicted_pc = fetched_predicted_pc;
    e.actual_next_pc = ins.pc + 1;
    e.op = ins.op;
    e.is_store = (ins.op == OpCode::SW);
    e.writes_reg = writesReg(ins.op);
    e.dest_reg = ins.dest;

    ROB[rob_tail] = e;

    int idx = rob_tail;
    rob_tail = (rob_tail + 1) % cfg.rob_size;
    rob_count++;

    return ROB[idx].tag;
}

int Processor::indexFromTag(int tag) {
    for (int i = 0; i < cfg.rob_size; i++) {
        if (ROB[i].busy && ROB[i].tag == tag) {
            return i;
        }
    }

    return -1;
}

bool Processor::writesReg(OpCode op) {
    return !(op == OpCode::SW || op == OpCode::BEQ || op == OpCode::BNE ||
             op == OpCode::BLT || op == OpCode::BLE || op == OpCode::J);
}

void Processor::fillOperands(RSEntry &entry, const Instruction &ins) {
    // src1 is always the boring part
    readSource(ins.src1, entry.ready1, entry.v1, entry.q1);

    if (ins.op == OpCode::ADDI || ins.op == OpCode::SLTI ||
        ins.op == OpCode::ANDI || ins.op == OpCode::ORI ||
        ins.op == OpCode::XORI) {
        entry.ready2 = true;
        entry.q2 = -1;
        entry.v2 = 0;
    } else {
        readSource(ins.src2, entry.ready2, entry.v2, entry.q2);
    }
}

void Processor::fillOperands(LSQEntry &entry, const Instruction &ins) {
    readSource(ins.src1, entry.ready1, entry.v1, entry.q1);

    if (ins.op == OpCode::SW) {
        readSource(ins.src2, entry.ready2, entry.v2, entry.q2);
    } else {
        entry.ready2 = true;
        entry.q2 = -1;
        entry.v2 = 0;
    }
}

void Processor::readSource(int reg, bool &ready, int &val, int &tag) {
    if (reg == -1 || reg == 0) {
        ready = true;
        val = 0;
        tag = -1;
        return;
    }

    if (decode_rat_snapshot[reg] == -1) {
        ready = true;
        val = decode_arf_snapshot[reg];
        tag = -1;
        return;
    }

    int idx = indexFromTagSnapshot(decode_rat_snapshot[reg]);

    if (idx != -1 && decode_rob_snapshot[idx].ready) {
        ready = true;
        val = decode_rob_snapshot[idx].value;
        tag = -1;
    } else {
        ready = false;
        tag = decode_rat_snapshot[reg];
    }
}

int Processor::indexFromTagSnapshot(int tag) const {
    for (int i = 0; i < cfg.rob_size; i++) {
        if (decode_rob_snapshot[i].busy && decode_rob_snapshot[i].tag == tag) {
            return i;
        }
    }

    return -1;
}

void Processor::writeResult(const UnitResult &out) {
    int idx = indexFromTag(out.tag);
    if (idx == -1) {
        return;
    }

    ROB[idx].ready = true;
    ROB[idx].has_exception = out.has_exception;

    if (ROB[idx].op == OpCode::SW) {
        ROB[idx].store_addr = out.value;
        ROB[idx].store_data = out.store_data;
    } else if (ROB[idx].op == OpCode::BEQ || ROB[idx].op == OpCode::BNE ||
               ROB[idx].op == OpCode::BLT || ROB[idx].op == OpCode::BLE) {
        ROB[idx].branch_taken = out.branch_taken;

        if (out.branch_taken) {
            ROB[idx].actual_next_pc = ROB[idx].pc + inst_memory[ROB[idx].pc].imm;
        } else {
            ROB[idx].actual_next_pc = ROB[idx].pc + 1;
        }
    } else if (ROB[idx].op == OpCode::J) {
        ROB[idx].actual_next_pc = ROB[idx].pc + inst_memory[ROB[idx].pc].imm;
    } else {
        ROB[idx].value = out.value;
    }

    for (auto &u : units) {
        u.capture(out.tag, out.value);
    }

    lsq->capture(out.tag, out.value);
}

void Processor::popROB() {
    ROB[rob_head] = ROBEntry();
    rob_head = (rob_head + 1) % cfg.rob_size;
    rob_count--;
}

bool Processor::pipelineEmpty() {
    if (fetched_valid || rob_count != 0) {
        return false;
    }

    for (auto &u : units) {
        if (!u.pipe.empty()) {
            return false;
        }

        for (auto &e : u.rs) {
            if (e.busy) {
                return false;
            }
        }
    }

    if (!lsq->pipe.empty() || !lsq->entries.empty()) {
        return false;
    }

    return true;
}
