#include "ooo_cpu.h"
#include "../fdip.h"

extern uint8_t pt;

FDIP fdip_prefetcher;

#define AHEADPRED
#define DISTAHEAD 10

#define NSHIFT 10
//a pseudo RNG (largely sufficient for the simulator)
static uint64_t RANDSEED = 0x3f79a17b4;

uint64_t
MYRANDOM() {
    uint64_t X = (RANDSEED >> 7) * 0x9745931;
    if (X == RANDSEED)
        X++;
    RANDSEED = X;
    return (RANDSEED & 127);
}

#define LOGMULTSIZE  (0)    //to test other sizes of predictors:  +1 doubles the size of MMA table and FNL tables


#define MMA_FILT_SIZE 24    // 24 entries in the MMA FILTER
static uint64_t PREVPRED[MMA_FILT_SIZE];    // the MMA prefetches
#define DISTAHEADMAX 80
static uint64_t PREVADDR[DISTAHEADMAX + 1];    //to memorize the previous addresses missing the I-Shadow cache
static uint64_t PREFCAND[DISTAHEADMAX + 1];
uint64_t PrefetchCandidate;
/// All variables  for FNL
#define MAXFNL 5        // 3 to 6  reaches approximately the same performance, but slightly more accesses to L2 with larger MAXFNL
#define PERIODRESET   8192
#define FNL_NBENTRIES (1<< (16+LOGMULTSIZE ))
static int WorthPF[FNL_NBENTRIES];
static int Touched[FNL_NBENTRIES];
static int ptReset;


#define NBWAYISHADOW 3
#define SIZESHADOWICACHE (64*NBWAYISHADOW)
static uint64_t ShadowICache[64][NBWAYISHADOW];


////////////////////////////

#define FITERFNLON
#define NBWAYFILTERFNL 4
#define SIZEWAYFILTERFNL 32
#define SIZEFILTERFNL (SIZEWAYFILTERFNL*NBWAYFILTERFNL)
static uint64_t JUSTNLPREFETCH[SIZEWAYFILTERFNL][NBWAYFILTERFNL];

void
JustFnl(uint64_t Block) {
//mark the block has just being fetch or prefetch (FIFO management)
    uint64_t set = (Block & (SIZEWAYFILTERFNL - 1));
    uint64_t tag = (Block / SIZEWAYFILTERFNL) & ((1 << 15) - 1);

    for (int i = NBWAYFILTERFNL - 1; i > 0; i--)
        JUSTNLPREFETCH[set][i] = JUSTNLPREFETCH[set][i - 1];
    JUSTNLPREFETCH[set][0] = tag;

}

bool
WasNotJustFnl(uint64_t Block) {
//check if the block has been fetched or prefetched
    uint64_t prev = Block - 1;
    int set = (prev & (SIZEWAYFILTERFNL - 1));
    uint64_t tag = (prev / SIZEWAYFILTERFNL) & ((1 << 15) - 1);
    for (int i = 0; i < NBWAYFILTERFNL; i++)
        if (JUSTNLPREFETCH[set][i] == tag)
            return false;
    return true;
}

///////////////////////////////

bool
IsInIShadow(uint64_t Block, bool Insert) {
// also manage replacement policy if Insert = true
    int Hit = -1;
    int set = Block & 63;
    uint64_t tag = (Block >> 6) & ((1 << 15) - 1);
    for (int i = 0; i < NBWAYISHADOW; i++)
        if (tag == ShadowICache[set][i]) {
            Hit = i;
            break;
        }
    if (Insert) {
        // Simple solution for software management of LRU
        int Max = (Hit != -1) ? Hit : NBWAYISHADOW - 1;
        for (int i = Max; i > 0; i--)
            ShadowICache[set][i] = ShadowICache[set][i - 1];
        ShadowICache[set][0] = tag;
    }
    return (Hit != -1);
}

////////////////////
#define NBWAYPRED 8
#define LOGTAGNEXTMISS 12    // 12 bit tags
#define LOGWAYNEXTMISS (10 +LOGMULTSIZE)
#define SIZEWAYNEXTMISS (1 << LOGWAYNEXTMISS)


uint64_t GNtag[NBWAYPRED * SIZEWAYNEXTMISS];
uint64_t GNblock[NBWAYPRED * SIZEWAYNEXTMISS];
int GNbMiss[NBWAYPRED * SIZEWAYNEXTMISS];
int8_t GU[NBWAYPRED * SIZEWAYNEXTMISS];

class PredictMiss {
public:
    uint64_t *Ntag;        // 12 bits
    uint64_t *NBlock;        // 58 bits
    int8_t *U;
    int *NbMiss;            // 1 bit for replacement and confidence
    int distahead;

    void init(int X) {
        Ntag = GNtag;
        NBlock = GNblock;
        NbMiss = GNbMiss;
        U = GU;
        for (int i = 0; i < NBWAYPRED * SIZEWAYNEXTMISS; i++) {
            U[i] = 0;
            Ntag[i] = 0;
            NBlock[i] = 0;
            NbMiss[i] = 0;
        }
        distahead = X;
    }

    uint64_t AheadPredict(uint64_t Addr) {
        PrefetchCandidate = 0;
        //  manage  the table as a skewed cache :-)
        int index[NBWAYPRED];
        int A = Addr & (SIZEWAYNEXTMISS - 1);
        int B = (Addr >> LOGWAYNEXTMISS) & (SIZEWAYNEXTMISS - 1);
        for (int i = 0; i < NBWAYPRED; i++) {
            index[i] = (A ^ B) + (i << LOGWAYNEXTMISS);
            A = (A >> 7) + ((A & 127) << (LOGWAYNEXTMISS - 7));
        }

        uint64_t tag = (Addr >> LOGWAYNEXTMISS) & ((1 << LOGTAGNEXTMISS) - 1);
        int NHIT = -1;
        for (int i = 0; i < NBWAYPRED; i++) {
            if (Ntag[index[i]] == tag) {
                NHIT = i;
                PrefetchCandidate = NBlock[index[NHIT]];
                break;
            }
        }
        if (NHIT == -1)
            return (0);
        uint64_t X = 0;
        if (U[index[NHIT]] > 0)    // there were at least two  misses on the same block
            X = NBlock[index[NHIT]];
        return (X);
    }

    void LinkAhead(uint64_t Block, uint64_t PrevAddr, uint8_t Hit) {
        // Fill the table  on I-cache miss
        if (Hit)
            return;
        int index[NBWAYPRED];
        int A = PrevAddr & (SIZEWAYNEXTMISS - 1);
        int B = (PrevAddr >> LOGWAYNEXTMISS) & (SIZEWAYNEXTMISS - 1);
        for (int i = 0; i < NBWAYPRED; i++) {
            index[i] = (A ^ B) + (i << LOGWAYNEXTMISS);
            A = (A >> 7) + ((A & 127) << (LOGWAYNEXTMISS - 7));

        }
        uint64_t tag = (PrevAddr >> LOGWAYNEXTMISS) & ((1 << LOGTAGNEXTMISS) - 1);
        int NHIT = -1;
        for (int i = 0; i < NBWAYPRED; i++) {
            if (Ntag[index[i]] == tag) {
                NbMiss[index[i]]++;
                NHIT = i;        //increase the confidence
                if (NBlock[index[i]] == Block) {

                    U[index[i]] = 1;
                } else {
                    NBlock[index[i]] = Block;
                    U[index[i]] = 0;


                };
                return;
            }
        }
        // let us try to allocate a new entry
        int X = MYRANDOM() % NBWAYPRED;
        for (int i = 0; i < NBWAYPRED; i++) {
            if (U[index[X]] == 0) {
                NHIT = X;
                break;
            };
            X = (X + 1) % NBWAYPRED;
        }
        if (NHIT == -1) {
            // decay some entry
            if ((MYRANDOM() & 15) == 0) {
                U[index[X]] = 0;
            }
        }
        if (NHIT != -1) {
            //allocate the entry
            NBlock[index[NHIT]] = Block;
            Ntag[index[NHIT]] = tag;
            NbMiss[index[NHIT]] = 0;
            U[index[X]] = 0;
        }
    }
};

PredictMiss AHEAD, AHEADphist;

#define    PrefCodeBlock(X) prefetch_code_line ((X)<<LOG2_BLOCK_SIZE)
// prefetch  works on  blocks

/////////////////////////////////
void O3_CPU::l1i_prefetcher_initialize() {
    cout << "IFETCH_BUFFER_SIZE = " << IFETCH_BUFFER_SIZE << endl;
    cout << "CPU " << cpu << " L1I fnl mma prefetcher" << endl;
    AHEAD.init(DISTAHEAD);
    AHEADphist.init(DISTAHEAD);
    if (IFETCH_BUFFER_SIZE == 192)
        fdip_prefetcher.initialize(cpu);
}


void O3_CPU::l1i_prefetcher_branch_operate(uint64_t ip, uint8_t branch_type, uint64_t branch_target,
                                           bool predicted_branch_taken, bool always_taken,
                                           uint64_t real_branch_target) {
    if (IFETCH_BUFFER_SIZE == 192) {
        fdip_prefetcher.branch_operate(ip, branch_type, branch_target,
                                       predicted_branch_taken, always_taken,
                                       real_branch_target);
    }
}

////////////////////
void O3_CPU::l1i_prefetcher_cache_operate(uint64_t v_addr,
                                     uint8_t cache_hit, uint8_t prefetch_hit) {
    //cout << "access v_addr: 0x" << hex << v_addr << dec << endl;
    uint64_t Block = v_addr >> LOG2_BLOCK_SIZE;
    int index = Block & (FNL_NBENTRIES - 1);
    bool ShadowMiss = (!IsInIShadow(Block, 1));
    uint64_t AheadPredictedBlock = 0;
// prefetch is triggered only on misses on the Shadow I-cache
    if (ShadowMiss) {
// The FNL prefetcher
/////// Manage if it is worth prefetching next block
        int previndex = (index - 1) & (FNL_NBENTRIES - 1);
        Touched[index] = 1;
        if (Touched[previndex])    //if ((index & 63)!=0)
        {            //the previous block was read not so long ago: it was worth prefetching this block
            if ((cache_hit == 0) || (WorthPF[previndex]))    // this allows to reduce the pressure on L2
                WorthPF[previndex] = 3;
        }

        for (int i = ptReset; i < ptReset + (FNL_NBENTRIES / PERIODRESET); i++)
// Once a block has become worth prefetching, it keeps this status for at least three intervals of PERIODRESET I-Shadow misses
        {

            if (Touched[i])
                if (WorthPF[i] > 0)
                    WorthPF[i]--;
            Touched[i] = 0;
        }
        ptReset += (FNL_NBENTRIES / PERIODRESET);
        ptReset &= (FNL_NBENTRIES - 1);

////////
// Next-line prefetch
        if (WorthPF[index] > 0) {
            bool NotJustAHEAD = true;
//verify that the block has not been already prefetched by MMA recently
            for (int i = MMA_FILT_SIZE - 1; i >= 0; i--)
                if (PREVPRED[i] == Block) {
                    NotJustAHEAD = false;
                    break;
                }
            if (NotJustAHEAD) {
                for (int i = 1; i <= MAXFNL; i++) {
                    uint64_t pf_Block = Block + i;
//if Block B-1 was accessed recently one has only to prefetch Block block+FNL
                    if ((WasNotJustFnl(Block)) || (i == MAXFNL)) {
                        PrefCodeBlock (pf_Block);
                    }
                    if (WorthPF[(index + i) & (FNL_NBENTRIES - 1)] == 0)
                        break;
                }
            }
#ifdef  FITERFNLON
            JustFnl(Block);
#endif
        }

/////// END OF THE FNL prefetcher

#ifdef AHEADPRED
/////

        AheadPredictedBlock =
                AHEADphist.AheadPredict((v_addr >> 2) ^ (PREVADDR[NSHIFT - 1] << 1));
        if (AheadPredictedBlock != 0) {
            bool NotJustMMA = true;
            for (int i = MMA_FILT_SIZE - 1; i >= 0; i--) {
                if (PREVPRED[i] == (AheadPredictedBlock)) {
                    NotJustMMA = false;
                    break;
                }
            }
            if (!NotJustMMA)
                AheadPredictedBlock = 0;
            if (NotJustMMA) {
                int index = (AheadPredictedBlock) & (FNL_NBENTRIES - 1);
// avoid issing prefetch the block if the previous block was prefetched

                PrefCodeBlock (AheadPredictedBlock);

                if (WorthPF[index] > 0) {
                    for (int i = 1; i <= MAXFNL; i++) {
                        uint64_t pf_Block = AheadPredictedBlock + i;
                        if ((WasNotJustFnl(AheadPredictedBlock))
                            || (i == MAXFNL))
                            PrefCodeBlock (pf_Block);
                        if (WorthPF[(index + i) & (FNL_NBENTRIES - 1)] == 0)
                            break;
                    }
#ifdef  FITERFNLON
                    JustFnl(AheadPredictedBlock);
#endif
                }

            }
        }

//First access resulted in a miss
        if (AheadPredictedBlock == 0) {
            //////////////

            AheadPredictedBlock = AHEAD.AheadPredict(v_addr >> 2);

            if (AheadPredictedBlock != 0) {
                bool NotJustMMA = true;
                for (int i = MMA_FILT_SIZE - 1; i >= 0; i--) {
                    if (PREVPRED[i] == (AheadPredictedBlock)) {
                        NotJustMMA = false;
                        break;
                    }
                }

                if (!NotJustMMA)
                    AheadPredictedBlock = 0;
                if (NotJustMMA) {
                    int index = (AheadPredictedBlock) & (FNL_NBENTRIES - 1);
// avoid issing prefetch the block if the previous block was prefetched

                    PrefCodeBlock (AheadPredictedBlock);

                    if (WorthPF[index] > 0) {
                        for (int i = 1; i <= MAXFNL; i++) {
                            uint64_t pf_Block = AheadPredictedBlock + i;
                            if ((WasNotJustFnl(AheadPredictedBlock))
                                || (i == MAXFNL))
                                PrefCodeBlock (pf_Block);
                            if (WorthPF[(index + i) & (FNL_NBENTRIES - 1)] == 0)
                                break;
                        }
#ifdef  FITERFNLON
                        JustFnl(AheadPredictedBlock);
#endif
                    }

                }
            }
        }            //else PrefetchCandidate=0;
/////
        if ((Block != (PREVADDR[0] >> 4) + 1) ||
            (MAXFNL == 0)) {            // Link Block to the address of the block that missed DISTAHEAD+1 before
            AHEAD.LinkAhead(Block, PREVADDR[AHEAD.distahead], cache_hit);

//the PC based  prefetch candidate  was not correct
            if ((PREFCAND[AHEAD.distahead] != 0) & (PREFCAND[AHEAD.distahead] !=
                                                    Block))
                AHEADphist.LinkAhead(Block,
                                     PREVADDR[AHEADphist.
                                             distahead] ^ (PREVADDR[AHEADphist.
                                             distahead +
                                                                    NSHIFT] <<
                                                                            1), cache_hit);
        }


        for (int i = DISTAHEADMAX; i > 0; i--)
            PREVADDR[i] = PREVADDR[i - 1];
        PREVADDR[0] = v_addr >> 2;
        for (int i = DISTAHEADMAX; i > 0; i--)
            PREFCAND[i] = PREFCAND[i - 1];
        PREFCAND[0] = PrefetchCandidate;


        if (AheadPredictedBlock != 0) {
            for (int i = MMA_FILT_SIZE - 1; i >= 1; i--)
                PREVPRED[i] = PREVPRED[i - 1];
            PREVPRED[0] = AheadPredictedBlock;
        }
#endif
    }
}

void O3_CPU::l1i_prefetcher_cycle_operate() {
    if (IFETCH_BUFFER_SIZE == 192)
        fdip_prefetcher.cycle_operate(this, pt);
}

void O3_CPU::l1i_prefetcher_cache_fill(uint64_t v_addr,
                                  uint32_t set, uint32_t way,
                                  uint8_t prefetch, uint64_t evicted_v_addr) {
    //cout << hex << "fill: 0x" << v_addr << dec << " " << set << " " << way << " " << (uint32_t)prefetch << " " << hex << "evict: 0x" << evicted_v_addr << dec << endl;
}

void O3_CPU::l1i_prefetcher_final_stats() {
    printf("I-Shadow cache %d bytes\n", (SIZESHADOWICACHE * (15 + 2)) / 8);
    printf("Touched + WorthPF tables %d bytes \n", (FNL_NBENTRIES * 3) / 8);
    printf("MMA filter %d bytes \n", (MMA_FILT_SIZE * 58) / 8);
    printf("FNL filter %d bytes \n", (SIZEFILTERFNL * (15 + 2)) / 8);
    printf("TOTAL PREFETCHER STORAGE SIZE: %d bytes\n",
           (71 * NBWAYPRED * SIZEWAYNEXTMISS / 8) +
           ((SIZESHADOWICACHE * (15 + 2)) / 8) + ((FNL_NBENTRIES * 3) / 8) +
           ((MMA_FILT_SIZE * 58) / 8) + ((SIZEFILTERFNL * (15 + 2) / 8)));
    cout << "CPU " << cpu << " L1I next line prefetcher final stats" << endl;
    if (IFETCH_BUFFER_SIZE == 192)
        fdip_prefetcher.final_stats(cpu);
}

void O3_CPU::l1i_prefetcher_resolved_branch_operate(ooo_model_instr &instr, bool decode_stage) {
    if (IFETCH_BUFFER_SIZE == 192) {
        fdip_prefetcher.resolved_branch(this, instr, decode_stage, pt);
    }
}
