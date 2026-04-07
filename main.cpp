#include <iostream>
#include <string>
#include "Processor.h"

using namespace std;

int main(int argc, char *argv[]) {
    if (argc < 2) {
        cerr << "Usage: ./main <filename.s> [-cycles N]\n";
        return 1;
    }

    int maxCycles = -1;
    if (argc == 4 && string(argv[2]) == "-cycles") {
        maxCycles = stoi(argv[3]);
    }

    ProcessorConfig config;
    Processor cpu(config);

    try {
        cpu.loadProgram(argv[1]);
    } catch (...) {
        cerr << "Failed to parse instruction file.\n";
        return 1;
    }

    int cycleCnt = 0;

    while (cpu.step()) {
        cycleCnt++;

        if (maxCycles != -1 && cycleCnt == maxCycles) {
            cout << "[!] Execution stopped at cycle limit: " << maxCycles << "\n";
            break;
        }
    }

    if (maxCycles == -1) {
        if (cpu.exception) {
            cout << "[+] Execution halted due to exception after " << cpu.clock_cycle << " cycles.\n";
        } else {
            cout << "[+] Execution complete naturally in " << cpu.clock_cycle << " cycles.\n";
        }
    }

    cpu.dumpArchitecturalState();

    // dump memory flat, nothing fancy
    for (size_t i = 0; i < cpu.Memory.size(); i++) {
        cout << cpu.Memory[i] << " ";
    }
    cout << endl;

    return 0;
}
