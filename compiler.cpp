#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <sstream>

using namespace std;

string trim(string s) {
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' ||
           s.back() == '\n' || s.back() == '\r')) {
        s.pop_back();
    }

    int i = 0;
    while (i < (int)s.size() && (s[i] == ' ' || s[i] == '\t')) {
        i++;
    }

    return s.substr(i);
}

string removeComment(string s) {
    int pos = (int)s.find('#');
    if (pos != -1) {
        s = s.substr(0, pos);
    }
    return trim(s);
}

bool isMemoryLine(string s) {
    return !s.empty() && s[0] == '.' && s.find(':') != string::npos;
}

vector<string> splitOperands(string s) {
    vector<string> parts;
    for (char &c : s) {
        if (c == ',') {
            c = ' ';
        }
    }

    string curr;
    stringstream ss(s);
    while (ss >> curr) {
        parts.push_back(curr);
    }

    return parts;
}

int countNumbers(string s) {
    int cnt = 0;
    string curr = "";

    for (char c : s) {
        if (c == ' ' || c == '\t') {
            if (!curr.empty()) {
                cnt++;
                curr = "";
            }
        } else {
            curr.push_back(c);
        }
    }

    if (!curr.empty()) {
        cnt++;
    }

    return cnt;
}

string replaceMemoryLabels(string line, map<string, int> &memLabel) {
    for (auto it : memLabel) {
        string oldPart = it.first + "(";
        string newPart = to_string(it.second) + "(";

        while (line.find(oldPart) != string::npos) {
            int pos = (int)line.find(oldPart);
            line.replace(pos, oldPart.size(), newPart);
        }
    }

    return line;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        return 1;
    }

    string filename = argv[1];
    ifstream fin(filename);

    if (!fin.is_open()) {
        return 1;
    }

    vector<string> lines;
    string line;

    while (getline(fin, line)) {
        lines.push_back(removeComment(line));
    }
    fin.close();

    map<string, int> memLabel;
    map<string, int> codeLabel;

    int memPtr = 0;

    // pass 1: memory labels
    for (string curr : lines) {
        if (curr.empty()) {
            continue;
        }
        if (!isMemoryLine(curr)) {
            continue;
        }

        int pos = (int)curr.find(':');
        string name = curr.substr(1, pos - 1);
        string rest = trim(curr.substr(pos + 1));

        memLabel[name] = memPtr;
        memPtr += countNumbers(rest);
    }

    int pc = 0;

    // pass 2: code labels
    for (string curr : lines) {
        if (curr.empty()) {
            continue;
        }
        if (isMemoryLine(curr)) {
            continue;
        }

        int pos = (int)curr.find(':');
        if (pos != -1) {
            string name = trim(curr.substr(0, pos));
            codeLabel[name] = pc;

            curr = trim(curr.substr(pos + 1));
            if (curr.empty()) {
                continue;
            }
        }

        pc++;
    }

    vector<string> out;
    pc = 0;

    // pass 3: rewrite instructions
    for (string curr : lines) {
        if (curr.empty()) {
            continue;
        }

        if (isMemoryLine(curr)) {
            out.push_back(curr);
            continue;
        }

        int pos = (int)curr.find(':');
        if (pos != -1) {
            curr = trim(curr.substr(pos + 1));
            if (curr.empty()) {
                continue;
            }
        }

        curr = replaceMemoryLabels(curr, memLabel);

        string op = curr;
        string rest = "";

        pos = (int)curr.find(' ');
        if (pos != -1) {
            op = curr.substr(0, pos);
            rest = trim(curr.substr(pos + 1));
        }

        vector<string> args = splitOperands(rest);

        if ((op == "beq" || op == "bne" || op == "blt" || op == "ble") &&
            (int)args.size() == 3) {
            if (codeLabel.find(args[2]) != codeLabel.end()) {
                int jump = codeLabel[args[2]] - pc;
                curr = op + " " + args[0] + ", " + args[1] + ", " + to_string(jump);
            }
        }

        if (op == "j" && (int)args.size() == 1) {
            if (codeLabel.find(args[0]) != codeLabel.end()) {
                int jump = codeLabel[args[0]] - pc;
                curr = op + " " + to_string(jump);
            }
        }

        out.push_back(curr);
        pc++;
    }

    ofstream fout(filename);
    if (!fout.is_open()) {
        return 1;
    }

    for (string curr : out) {
        fout << curr << "\n";
    }

    fout.close();
    return 0;
}
