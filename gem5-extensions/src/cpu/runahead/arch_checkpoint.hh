#ifndef __CPU_RUNAHEAD_ARCH_CHECKPOINT_HH__
#define __CPU_RUNAHEAD_ARCH_CHECKPOINT_HH__

#include "cpu/runahead/regfile.hh"
#include "cpu/runahead/rename_map.hh"
#include "cpu/runahead/free_list.hh"
#include "cpu/runahead/scoreboard.hh"
#include "params/BaseRunaheadCPU.hh"

namespace gem5
{
namespace runahead
{

class CPU;

/**
 * Architectural state checkpoint used to save state before entering runahead and
 * after exiting it in order to restore the CPU to its observable state just before runahead.
 * 
 * Mainly, this checkpoints the architectural registers. Other physical registers are ignored
 * as the CPU must resume at fetch after exiting runahead anyways, so rename will reclaim
 * all other physical registers.
 */
class ArchCheckpoint
{
private:
    struct RegCheckpoint {
        /** The checkpointed values */
        std::vector<RegVal> values;
        /** Indices of valid checkpoint */
        std::list<RegIndex> validIdxs;
    };

    /** The CPU whose state is checkpointed */
    CPU *cpu;

    /** The amount of threads in use */
    ThreadID numThreads;

    /**
     * Checkpointed architectural register values
     * Index into this with the register class, then register arch index
     */
    std::array<RegCheckpoint, MiscRegClass + 1> registerCheckpoints;

public:
    ArchCheckpoint(CPU *cpu, const BaseRunaheadCPUParams &params);

    /** Restore the architectural state of the CPU */
    void restore(ThreadID tid);

    /**
     * Checkpoints /all/ registers at once
     * This includes normal registers and all valid miscellaneous registers
     */
    void fullSave(ThreadID tid);

    /**
     * Update the checkpoint of a single architectural register
     * Looks up the current physical register tied to the arch registers and saves it
     */
    void updateReg(ThreadID tid, RegId archReg);
};

} // namespace runahead
} // namespace gem5

#endif // __CPU_RUNAHEAD_ARCH_CHECKPOINT_HH__