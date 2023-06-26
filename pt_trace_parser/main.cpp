//
// Created by Shixin Song on 2021/8/2.
//

#include "instruction.h"
#include <iostream>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <string>
#include <fstream>
#include <zlib.h>
#include <memory>
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;

extern "C" {
#include <xed/xed-interface.h>
}

#define GZ_BUFFER_SIZE 80

using std::cout;
using std::endl;
using std::cerr;
using std::pair;
using std::unordered_set;
using std::unordered_map;
using std::vector;

static bool xedInitDone = false;


std::unique_ptr<xed_decoded_inst_t> makeNop(uint8_t _length) {
    // A 10-to-15-byte NOP instruction (direct XED support is only up to 9)
    static const char *nop15 =
            "\x66\x66\x66\x66\x66\x66\x2e\x0f\x1f\x84\x00\x00\x00\x00\x00";

    auto ptr = std::make_unique<xed_decoded_inst_t>();
    xed_decoded_inst_t *ins = ptr.get();
//        xed_decoded_inst_zero_set_mode(ins, &xed_state_);
    xed_decoded_inst_zero(ins);
    xed_decoded_inst_set_mode(ins, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
    xed_error_enum_t res;

    // The reported instruction length must be 1-15 bytes
    _length &= 0xf;
    assert(_length > 0);
    if (_length > 9) {
        int offset = 15 - _length;
        const auto *pos = reinterpret_cast<const uint8_t *>(nop15 + offset);
        res = xed_decode(ins, pos, 15 - offset);
    } else {
        uint8_t buf[10];
        res = xed_encode_nop(&buf[0], _length);
        if (res != XED_ERROR_NONE) {
            cerr << "XED NOP encode error: " << xed_error_enum_t2str(res);
        }
        res = xed_decode(ins, buf, sizeof(buf));
    }
    if (res != XED_ERROR_NONE) {
        cerr << "XED NOP encode error: " << xed_error_enum_t2str(res);
    }
    return ptr;
}


void read_one_file(fs::path &trace_path, vector<double> &bounds) {
    // Return (baseline set in num of cache lines, num of unique branches)
    unordered_set<uint64_t> basic_branch_set;
    unordered_set<uint64_t> instruction_set;
    unordered_map<uint64_t, uint64_t> cache_line_access_count;

    auto trace_file = gzopen(trace_path.c_str(), "rb");
    assert(trace_file != nullptr);
    if (!xedInitDone) {
        xed_tables_init();
        xedInitDone = true;
    }
    char buffer[GZ_BUFFER_SIZE];
    while (gzgets(trace_file, buffer, GZ_BUFFER_SIZE) != Z_NULL) {
        pt_instr trace_read_instr_pt(buffer);
        if (trace_read_instr_pt.pc == 0) continue;

        ooo_model_instr arch_instr;
        arch_instr.ip = trace_read_instr_pt.pc;
        arch_instr.size = trace_read_instr_pt.size;
        xed_decoded_inst_zero(&arch_instr.inst_pt);
        xed_decoded_inst_set_mode(&arch_instr.inst_pt, XED_MACHINE_MODE_LONG_64, XED_ADDRESS_WIDTH_64b);
        xed_error_enum_t xed_error = xed_decode(&arch_instr.inst_pt, trace_read_instr_pt.inst_bytes.data(),
                                                trace_read_instr_pt.size);
        if (xed_error != XED_ERROR_NONE) {
            arch_instr.inst_pt = *makeNop(arch_instr.size);
        }
        // determine what kind of branch this is, if any
        arch_instr.is_branch = 1;
        auto category = xed_decoded_inst_get_category(&arch_instr.inst_pt);
        switch (category) {
            case XED_CATEGORY_COND_BR:
                arch_instr.branch_type = BRANCH_CONDITIONAL;
                break;
            case XED_CATEGORY_UNCOND_BR:
                if (xed3_operand_get_brdisp_width(&arch_instr.inst_pt))
                    arch_instr.branch_type = BRANCH_DIRECT_JUMP;
                else
                    arch_instr.branch_type = BRANCH_INDIRECT;
                break;
            case XED_CATEGORY_CALL:
                if (xed3_operand_get_brdisp_width(&arch_instr.inst_pt))
                    arch_instr.branch_type = BRANCH_DIRECT_CALL;
                else
                    arch_instr.branch_type = BRANCH_INDIRECT_CALL;
                break;
            case XED_CATEGORY_RET:
                arch_instr.branch_type = BRANCH_RETURN;
                break;
            default:
                arch_instr.branch_type = NOT_BRANCH;
                arch_instr.is_branch = NOT_BRANCH;
                break;
        }

        uint64_t cl_addr_start = arch_instr.ip >> 6;
        uint64_t cl_addr_end = (arch_instr.ip + arch_instr.size - 1) >> 6;
        for (uint64_t i = cl_addr_start; i <= cl_addr_end; i++) {
            auto it = cache_line_access_count.find(i);
            if (it == cache_line_access_count.end()) {
                cache_line_access_count.emplace_hint(it, i, 0);
                it = cache_line_access_count.find(i);
            }
            it->second++;
        }
        if (arch_instr.branch_type != NOT_BRANCH &&
            arch_instr.branch_type != BRANCH_RETURN &&
            arch_instr.branch_type != BRANCH_INDIRECT &&
            arch_instr.branch_type != BRANCH_INDIRECT_CALL) {
            basic_branch_set.insert(arch_instr.ip);
        }
        instruction_set.insert(arch_instr.ip);
    }
    vector<uint64_t> cache_line_count;
    cache_line_count.reserve(cache_line_access_count.size());
    for (auto &it : cache_line_access_count) {
        cache_line_count.push_back(it.second);
    }
    std::sort(cache_line_count.begin(), cache_line_count.end(), std::greater<>());
    uint64_t curr_access = 0, all_access = 0;
    vector<uint64_t> cdf;
    cdf.reserve(cache_line_count.size());
    for (auto a : cache_line_count) {
        curr_access += a;
        cdf.push_back(curr_access);
        all_access += a;
    }
    cout << trace_path.parent_path().stem() << ",";
    for (auto bound : bounds) {
        auto lower = std::lower_bound(cdf.begin(), cdf.end(), ceil(bound * ((double) all_access)));
        cout << std::distance(cdf.begin(), lower) + 1 << ",";
    }
    cout << cdf.size() << ",";
    cout << basic_branch_set.size() << "," << instruction_set.size() << endl;
}


int main() {
    fs::path all_trace_dir = "/mnt/storage/isaachyw/champsim_pt/pt_traces";
    cout << "Trace,0.9,0.95,0.99,1,num branches,num instructions" << endl;
    vector<double> bounds = {0.9, 0.95, 0.99};
    for (auto &trace_dir : fs::directory_iterator(all_trace_dir)) {
        auto trace_path = trace_dir.path() / "trace.gz";
        read_one_file(trace_path, bounds);
    }
    return 0;
}
