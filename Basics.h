#pragma once
#include <string>

// opcodes supported in the simulator
enum class OpCode {
    ADD, SUB, ADDI, MUL, DIV, REM, LW, SW,
    BEQ, BNE, BLT, BLE, J,
    SLT, SLTI, AND, OR, XOR, ANDI, ORI, XORI
};

// execution unit types
enum class UnitType {
    ADDER,
    MULTIPLIER,
    DIVIDER,
    LOADSTORE,
    BRANCH,
    LOGIC
};

// one parsed instruction
struct Instruction {
    OpCode op = OpCode::ADD;
    int dest = -1;
    int src1 = -1;
    int src2 = -1;
    int imm = 0;
    int pc = 0;
};

// config of processor
struct ProcessorConfig {
    int num_regs = 32;
    int rob_size = 64;
    int mem_size = 1024;

    int logic_lat = 1;
    int add_lat = 2;
    int mul_lat = 4;
    int div_lat = 5;
    int mem_lat = 4;

    int logic_rs_size = 4;
    int adder_rs_size = 4;
    int mult_rs_size = 2;
    int div_rs_size = 2;
    int br_rs_size = 2;
    int lsq_rs_size = 32;
};

// one ROB entry
struct ROBEntry {
    bool busy = false;
    bool ready = false;
    bool has_exception = false;

    int tag = -1;
    int pc = -1;
    int predicted_pc = -1;
    int actual_next_pc = -1;

    OpCode op = OpCode::ADD;

    bool writes_reg = false;
    int dest_reg = -1;
    int value = 0;

    bool is_store = false;
    int store_addr = 0;
    int store_data = 0;

    bool branch_taken = false;
};

// one RS entry
struct RSEntry {
    bool busy = false;
    int order_pc = -1;
    int dest_tag = -1;
    OpCode op = OpCode::ADD;
    int imm = 0;

    bool ready1 = false;
    bool ready2 = false;
    int v1 = 0;
    int v2 = 0;
    int q1 = -1;
    int q2 = -1;
};
