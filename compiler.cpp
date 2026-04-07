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
    int p = (int)s.find('#');
    if (p != -1) {
        s = s.substr(0, p);
    }
    return trim(s);
}

bool isMemoryLine(string s) {
    return !s.empty() && s[0] == '.' && s.find(':') != string::npos;
}

vector<string> splitOperands(string s) {
    vector<string> out;
    for (char &c : s) {
        if (c == ',') {
            c = ' ';
        }
    }

    string x;
    stringstream ss(s);
    while (ss >> x) {
        out.push_back(x);
    }

    return out;
}

int countNumbers(string s) {
    int cnt = 0;
    string cur = "";

    for (char c : s) {
        if (c == ' ' || c == '\t') {
            if (!cur.empty()) {
                cnt++;
                cur = "";
            }
        } else {
            cur.push_back(c);
        }
    }

    if (!cur.empty()) {
        cnt++;
    }

    return cnt;
}

string replaceMemoryLabels(string line, map<string, int> &memLabel) {
    for (auto it : memLabel) {
        string oldStr = it.first + "(";
        string newStr = to_string(it.second) + "(";

        while (line.find(oldStr) != string::npos) {
            int p = (int)line.find(oldStr);
            line.replace(p, oldStr.size(), newStr);
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

    // first pass, data labels
    for (string s : lines) {
        if (s.empty()) {
            continue;
        }
        if (!isMemoryLine(s)) {
            continue;
        }

        int p = (int)s.find(':');
        string name = s.substr(1, p - 1);
        string rest = trim(s.substr(p + 1));

        memLabel[name] = memPtr;
        memPtr += countNumbers(rest);
    }

    int pc = 0;

    // second pass, code labels
    for (string s : lines) {
        if (s.empty()) {
            continue;
        }
        if (isMemoryLine(s)) {
            continue;
        }

        int p = (int)s.find(':');
        if (p != -1) {
            string name = trim(s.substr(0, p));
            codeLabel[name] = pc;

            s = trim(s.substr(p + 1));
            if (s.empty()) {
                continue;
            }
        }

        pc++;
    }

    vector<string> out;
    pc = 0;

    // now fix jumps/labels
    for (string s : lines) {
        if (s.empty()) {
            continue;
        }

        if (isMemoryLine(s)) {
            out.push_back(s);
            continue;
        }

        int p = (int)s.find(':');
        if (p != -1) {
            s = trim(s.substr(p + 1));
            if (s.empty()) {
                continue;
            }
        }

        s = replaceMemoryLabels(s, memLabel);

        string op = s;
        string rest = "";

        p = (int)s.find(' ');
        if (p != -1) {
            op = s.substr(0, p);
            rest = trim(s.substr(p + 1));
        }

        vector<string> args = splitOperands(rest);

        if ((op == "beq" || op == "bne" || op == "blt" || op == "ble") &&
            (int)args.size() == 3) {
            if (codeLabel.find(args[2]) != codeLabel.end()) {
                int jump = codeLabel[args[2]] - pc;
                s = op + " " + args[0] + ", " + args[1] + ", " + to_string(jump);
            }
        }

        if (op == "j" && (int)args.size() == 1) {
            if (codeLabel.find(args[0]) != codeLabel.end()) {
                int jump = codeLabel[args[0]] - pc;
                s = op + " " + to_string(jump);
            }
        }

        out.push_back(s);
        pc++;
    }

    ofstream fout(filename);
    if (!fout.is_open()) {
        return 1;
    }

    for (string s : out) {
        fout << s << "\n";
    }

    fout.close();
    return 0;
}
