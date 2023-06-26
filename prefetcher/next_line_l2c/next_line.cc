#include "cache.h"

void CACHE::l2c_prefetcher_initialize() 
{
    cout << "CPU " << cpu << " L2C next line prefetcher" << endl;
}

uint32_t CACHE::l2c_prefetcher_operate(uint64_t v_addr, uint64_t addr, uint64_t ip, uint8_t cache_hit, uint8_t type, uint32_t metadata_in)
{
    uint64_t pf_addr = ((addr>>LOG2_BLOCK_SIZE)+1) << LOG2_BLOCK_SIZE;

    DP ( if (warmup_complete[cpu]) {
    cout << "[" << NAME << "] " << __func__ << hex << " base_cl: " << (addr>>LOG2_BLOCK_SIZE);
    cout << " pf_cl: " << (pf_addr>>LOG2_BLOCK_SIZE) << " ip: " << ip << " cache_hit: " << +cache_hit << " type: " << +type << endl; });

    prefetch_line(ip, addr, pf_addr, FILL_L2, 0);

    return metadata_in;
}

uint32_t CACHE::l2c_prefetcher_cache_fill(uint64_t v_addr, uint64_t addr, uint32_t set, uint32_t way, uint8_t prefetch, uint64_t evicted_addr, uint32_t metadata_in)
{
  return metadata_in;
}

void CACHE::l2c_prefetcher_final_stats()
{
    cout << "CPU " << cpu << " L2C next line prefetcher final stats" << endl;
}
