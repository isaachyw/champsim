/***
 * THIS FILE IS AUTOMATICALLY GENERATED
 * Do not edit this file. It will be overwritten when cmake is run.
 ***/

#ifndef CHAMPSIM_CONSTANTS_H
#define CHAMPSIM_CONSTANTS_H

#include "util.h"

// global

#define BLOCK_SIZE 64u
#define LOG2_BLOCK_SIZE lg2(BLOCK_SIZE)
#define PAGE_SIZE 4096u
#define LOG2_PAGE_SIZE lg2(PAGE_SIZE)
#define STAT_PRINTING_PERIOD 10000000u
#define NUM_CPUS 1u
#define CPU_FREQ 4000u

// ooo_cpu

#define IFETCH_BUFFER_SIZE 192u
#define DECODE_BUFFER_SIZE 60u
#define DISPATCH_BUFFER_SIZE 32u
#define ROB_SIZE 352u
#define LQ_SIZE 128u
#define SQ_SIZE 72u
#define FETCH_WIDTH 6u
#define DECODE_WIDTH 6u
#define DISPATCH_WIDTH 6u
#define EXEC_WIDTH 6u
#define LQ_WIDTH 2u
#define SQ_WIDTH 2u
#define RETIRE_WIDTH 5u
#define BRANCH_MISPREDICT_PENALTY 1u
#define SCHEDULER_SIZE 128u
#define DECODE_LATENCY 1u
#define DISPATCH_LATENCY 1u
#define SCHEDULING_LATENCY 0u
#define EXEC_LATENCY 0u

// DIB

#define DIB_WINDOW_SIZE 16u
#define LOG2_DIB_WINDOW_SIZE lg2(DIB_WINDOW_SIZE)
#define DIB_SET 32u
#define DIB_WAY 8u

// L1I

#define L1I_SET 64u
#define L1I_WAY 8u
#define L1I_WQ_SIZE 64u
#define L1I_RQ_SIZE 64u
#define L1I_PQ_SIZE 32u
#define L1I_MSHR_SIZE 16u
#define L1I_HIT_LATENCY 3u
#define L1I_FILL_LATENCY 1u
#define L1I_MAX_READ 2
#define L1I_MAX_WRITE 2

// L1D

#define L1D_SET 64u
#define L1D_WAY 12u
#define L1D_WQ_SIZE 64u
#define L1D_RQ_SIZE 64u
#define L1D_PQ_SIZE 8u
#define L1D_MSHR_SIZE 16u
#define L1D_HIT_LATENCY 4u
#define L1D_FILL_LATENCY 1u
#define L1D_MAX_READ 2
#define L1D_MAX_WRITE 2

// L2C

#define L2C_SET 1024u
#define L2C_WAY 8u
#define L2C_WQ_SIZE 32u
#define L2C_RQ_SIZE 32u
#define L2C_PQ_SIZE 16u
#define L2C_MSHR_SIZE 32u
#define L2C_HIT_LATENCY 9u
#define L2C_FILL_LATENCY 1u
#define L2C_MAX_READ 1
#define L2C_MAX_WRITE 1

// ITLB

#define ITLB_SET 16u
#define ITLB_WAY 4u
#define ITLB_WQ_SIZE 16u
#define ITLB_RQ_SIZE 16u
#define ITLB_PQ_SIZE 0u
#define ITLB_MSHR_SIZE 8u
#define ITLB_HIT_LATENCY 0u
#define ITLB_FILL_LATENCY 1u
#define ITLB_MAX_READ 2
#define ITLB_MAX_WRITE 2

// DTLB

#define DTLB_SET 16u
#define DTLB_WAY 4u
#define DTLB_WQ_SIZE 16u
#define DTLB_RQ_SIZE 16u
#define DTLB_PQ_SIZE 0u
#define DTLB_MSHR_SIZE 8u
#define DTLB_HIT_LATENCY 0u
#define DTLB_FILL_LATENCY 1u
#define DTLB_MAX_READ 2
#define DTLB_MAX_WRITE 2

// STLB

#define STLB_SET 128u
#define STLB_WAY 12u
#define STLB_WQ_SIZE 32u
#define STLB_RQ_SIZE 32u
#define STLB_PQ_SIZE 0u
#define STLB_MSHR_SIZE 16u
#define STLB_HIT_LATENCY 7u
#define STLB_FILL_LATENCY 1u
#define STLB_MAX_READ 1
#define STLB_MAX_WRITE 1

// LLC

#define LLC_SET 2048u
#define LLC_WAY 16u
#define LLC_WQ_SIZE 32u
#define LLC_RQ_SIZE 32u
#define LLC_PQ_SIZE 32u
#define LLC_MSHR_SIZE 64u
#define LLC_HIT_LATENCY 19u
#define LLC_FILL_LATENCY 1u
#define LLC_MAX_READ 1
#define LLC_MAX_WRITE 1

// physical_memory

#define DRAM_IO_FREQ 3200u
#define DRAM_CHANNELS 1u
#define LOG2_DRAM_CHANNELS lg2(DRAM_CHANNELS)
#define DRAM_RANKS 1u
#define LOG2_DRAM_RANKS lg2(DRAM_RANKS)
#define DRAM_BANKS 8u
#define LOG2_DRAM_BANKS lg2(DRAM_BANKS)
#define DRAM_ROWS 65536u
#define LOG2_DRAM_ROWS lg2(DRAM_ROWS)
#define DRAM_COLUMNS 128u
#define LOG2_DRAM_COLUMNS lg2(DRAM_COLUMNS)
#define DRAM_ROW_SIZE 8u
#define DRAM_CHANNEL_WIDTH 8u
#define DRAM_WQ_SIZE 64u
#define DRAM_RQ_SIZE 64u
#define tRP_DRAM_NANOSECONDS 12.5
#define tRCD_DRAM_NANOSECONDS 12.5
#define tCAS_DRAM_NANOSECONDS 12.5

// virtual memory

#define VIRTUAL_MEMORY_SIZE 8589934592
#define VIRTUAL_MEMORY_NUM_LEVELS 5


#endif
