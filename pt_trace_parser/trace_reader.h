//
// Created by isaachywong on 2/7/22.
//

#ifndef CHAMPSIM_PT_TRACE_READER_H
#define CHAMPSIM_PT_TRACE_READER_H

#include <cstdint>
#include <vector>
#include <string>
#include <bits/stdc++.h>
#include <zlib.h>
#include <boost/algorithm/string.hpp>
#include <cstring>
#include <boost/filesystem.hpp>
#include <iostream>
#include <sstream>
namespace fs = boost::filesystem;

extern "C" {
#include <xed/xed-interface.h>
}

using std::string;
using std::vector;
using std::istringstream;

enum class InstrType {
    IsBranch, // If we use this as InstrType, means we don't care the specific branch type
    NotBranch
};

enum class BranchType {
    DontCare
};

struct Instr {
    uint64_t ip;
    uint64_t size;
    InstrType instr_type;
    BranchType branch_type;
    xed_decoded_inst_t inst_pt;
    std::vector<uint8_t> inst_bytes;

    Instr(string &line, uint64_t ip_index, uint64_t size_index, uint64_t instr_index) {
        // Separate line
        boost::trim_if(line, boost::is_any_of("\n"));
        boost::trim_if(line, boost::is_any_of(" "));
        vector<string> parsed;
        boost::split(parsed, line, boost::is_any_of(" "), boost::token_compress_on);

        // Init values
        ip = hex_string_to_u64(parsed[ip_index]);
        size = hex_string_to_u64(parsed[size_index]);
        assert(parsed.size() >= size + instr_index);
        for (uint64_t i = 0; i < size; i++) {
            inst_bytes.push_back(hex_string_to_u64(parsed[instr_index + i]));
        }
    }

    static uint64_t hex_string_to_u64(string &tmp) {
        if (tmp.rfind("0x", 0) == 0) {
            //
        } else {
            tmp = "0x" + tmp;
        }
        istringstream iss(tmp);
        uint64_t value;
        iss >> std::hex >> value;
        return value;
    }
};

class TraceReader {
public:
    TraceReader();
};

#endif //CHAMPSIM_PT_TRACE_READER_H
