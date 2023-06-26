#include "cache.h"

#define maxRRPV 3
static uint32_t rrpv[L1D_SET][L1D_WAY];

// initialize replacement state
void CACHE::l1d_initialize_replacement()
{
    cout << "Initialize SRRIP state on L1D" << endl;

    for (int i=0; i<L1D_SET; i++) {
        for (int j=0; j<L1D_WAY; j++) {
            rrpv[i][j] = maxRRPV;
        }
    }
}

// find replacement victim
uint32_t CACHE::l1d_find_victim(uint32_t cpu, uint64_t instr_id, uint32_t set, const BLOCK *current_set, uint64_t ip, uint64_t full_addr, uint32_t type)
{
    // look for the maxRRPV line
    while (1)
    {
        for (int i=0; i<L1D_WAY; i++)
            if (rrpv[set][i] == maxRRPV)
                return i;

        for (int i=0; i<L1D_WAY; i++)
            rrpv[set][i]++;
    }

    // WE SHOULD NOT REACH HERE
    assert(0);
    return 0;
}

// called on every cache hit and cache fill
void CACHE::l1d_update_replacement_state(uint32_t cpu, uint32_t set, uint32_t way, uint64_t full_addr, uint64_t ip, uint64_t victim_addr, uint32_t type, uint8_t hit)
{
    string TYPE_NAME;
    if (type == LOAD)
        TYPE_NAME = "LOAD";
    else if (type == RFO)
        TYPE_NAME = "RFO";
    else if (type == PREFETCH)
        TYPE_NAME = "PF";
    else if (type == WRITEBACK)
        TYPE_NAME = "WB";
    else
        assert(0);

    if (hit)
        TYPE_NAME += "_HIT";
    else
        TYPE_NAME += "_MISS";

    if ((type == WRITEBACK) && ip)
        assert(0);

    // uncomment this line to see the LLC accesses
    // cout << "CPU: " << cpu << "  LLC " << setw(9) << TYPE_NAME << " set: " << setw(5) << set << " way: " << setw(2) << way;
    // cout << hex << " paddr: " << setw(12) << paddr << " ip: " << setw(8) << ip << " victim_addr: " << victim_addr << dec << endl;
    
    if (hit)
        rrpv[set][way] = 0;
    else
        rrpv[set][way] = maxRRPV-1;
}

// use this function to print out your own stats at the end of simulation
void CACHE::l1d_replacement_final_stats()
{

}
