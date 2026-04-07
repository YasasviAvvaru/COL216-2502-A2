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

bool LoadStoreQueue::addEntry(const LSQEntry &entry) {
    if (!hasFreeRS()) {
        return false;
    }

    entries.push_back(entry);
    return true;
}

void LoadStoreQueue::capture(int tag, int val) {
    for (auto &entry : entries) {
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
            for (const auto &job : pipe) {
                if (job.op == OpCode::SW &&
                    job.base + job.imm == addr &&
                    job.left >= latency) {
                    ok = false;
                    break;
                }
            }
        }

        if (ok) {
            LSQJob job;
            job.tag = entries[0].dest_tag;
            job.op = entries[0].op;
            job.base = entries[0].v1;
            job.imm = entries[0].imm;
            job.store_data = entries[0].v2;
            job.left = latency;

            pipe.push_back(job);
            entries.erase(entries.begin());
        }
    }

    for (auto &job : pipe) {
        job.left--;
    }

    std::vector<LSQJob> next_pipe;
    for (auto &job : pipe) {
        if (job.left == 0) {
            UnitResult out;
            out.tag = job.tag;

            int addr = job.base + job.imm;

            if (addr < 0 || addr >= (int)memory.size()) {
                out.has_exception = true;
            } else if (job.op == OpCode::LW) {
                int forwarded_tag = -1;
                int forwarded_value = 0;

                for (const auto &entry : rob) {
                    if (!entry.busy || !entry.ready || !entry.is_store || entry.has_exception) {
                        continue;
                    }
                    if (entry.tag >= job.tag || entry.store_addr != addr) {
                        continue;
                    }
                    if (entry.tag > forwarded_tag) {
                        forwarded_tag = entry.tag;
                        forwarded_value = entry.store_data;
                    }
                }

                if (forwarded_tag != -1) {
                    out.value = forwarded_value;
                } else {
                    out.value = memory[addr];
                }
            } else {
                out.value = addr;
                out.store_data = job.store_data;
            }

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
