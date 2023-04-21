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
 * 
 * For performance reasons, the branch predictor's branch history and return address stack are
 * also saved in architectural checkpoints.
 */
class ArchCheckpoint
{
private:
    /** The CPU whose state is checkpointed */
    CPU *cpu;

    /** Architectural register checkpoint file */
    PhysRegFile regFile;

    /** Hardcoded/predetermined rename maps to use with the checkpointed register file */
    UnifiedRenameMap renameMap;

    /** Hardcoded/predetermined free reg list */
    UnifiedFreeList freeList;

    /** Hardcoded/predetermined scoreboard of free registers */
    Scoreboard scoreboard;

    /** Starting flat indices per register type */
    std::array<uint16_t, CCRegClass> typeFlatIndices;

    /** Stored misc register values */
    std::vector<RegVal> miscRegs;

public:
    ArchCheckpoint(CPU *cpu, const BaseRunaheadCPUParams &params);

    /** Save the full current architectural state of the CPU */
    void fullSave(ThreadID tid);

    /** Restore the architectural state of the CPU */
    void restore(ThreadID tid);

    /** Update the checkpoint of a single architectural register */
    void updateReg(ThreadID tid, RegId archReg);
};

} // namespace runahead
} // namespace gem5

#endif // __CPU_RUNAHEAD_ARCH_CHECKPOINT_HH__