#include "LoadStoreQueue.h"

LoadStoreQueue::LoadStoreQueue() {
}

LoadStoreQueue::LoadStoreQueue(int lat, int sz) {
    latency = lat;
    rs_size = sz;
}

bool LoadStoreQueue::hasFreeRS() const {
    return (int)entries.size() + (int)pipe.size() < rs_size;
}

bool LoadStoreQueue::addEntry(const LSQEntry &e) {
    if (!hasFreeRS()) {
        return false;
    }

    entries.push_back(e);
    return true;
}

void LoadStoreQueue::capture(int tag, int val) {
    for (auto &x : entries) {
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

void LoadStoreQueue::executeCycle(std::vector<int> &memory, const std::vector<ROBEntry> &rob) {
    finished.clear();
    has_result = false;
    has_exception = false;

    if (!entries.empty()) {
        bool ok = entries[0].busy && entries[0].ready1;

        if (entries[0].op == OpCode::SW) {
            ok = ok && entries[0].ready2;
        } else if (ok) {
            int addr = entries[0].v1 + entries[0].imm;
            for (const auto &x : pipe) {
                if (x.op == OpCode::SW &&
                    x.base + x.imm == addr &&
                    x.left >= latency) {
                    ok = false;
                    break;
                }
            }
        }

        if (ok) {
            LSQJob cur;
            cur.tag = entries[0].dest_tag;
            cur.op = entries[0].op;
            cur.base = entries[0].v1;
            cur.imm = entries[0].imm;
            cur.store_data = entries[0].v2;
            cur.left = latency;

            pipe.push_back(cur);
            entries.erase(entries.begin());
        }
    }

    for (auto &x : pipe) {
        x.left--;
    }

    std::vector<LSQJob> nxt;
    for (auto &x : pipe) {
        if (x.left == 0) {
            UnitResult out;
            out.tag = x.tag;

            int addr = x.base + x.imm;

            if (addr < 0 || addr >= (int)memory.size()) {
                out.has_exception = true;
            } else if (x.op == OpCode::LW) {
                int bestTag = -1;
                int bestVal = 0;

                // latest older store wins
                for (const auto &r : rob) {
                    if (!r.busy || !r.ready || !r.is_store || r.has_exception) {
                        continue;
                    }
                    if (r.tag >= x.tag || r.store_addr != addr) {
                        continue;
                    }
                    if (r.tag > bestTag) {
                        bestTag = r.tag;
                        bestVal = r.store_data;
                    }
                }

                if (bestTag != -1) {
                    out.value = bestVal;
                } else {
                    out.value = memory[addr];
                }
            } else {
                out.value = addr;
                out.store_data = x.store_data;
            }

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
