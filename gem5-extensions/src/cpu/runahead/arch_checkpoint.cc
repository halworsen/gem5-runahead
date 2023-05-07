#include "cpu/runahead/arch_checkpoint.hh"
#include "cpu/runahead/cpu.hh"
#include "config/the_isa.hh"
#include "cpu/reg_class.hh"
#include "debug/RunaheadCheckpoint.hh"
#include "params/BaseRunaheadCPU.hh"

namespace gem5
{
namespace runahead
{

ArchCheckpoint::ArchCheckpoint(CPU *cpu, const BaseRunaheadCPUParams &params) :
    cpu(cpu),
    regFile(params.isa[0]->regClasses().at(IntRegClass).numRegs(),
            params.isa[0]->regClasses().at(FloatRegClass).numRegs(),
            params.isa[0]->regClasses().at(VecRegClass).numRegs(),
            params.isa[0]->regClasses().at(VecPredRegClass).numRegs(),
            params.isa[0]->regClasses().at(CCRegClass).numRegs(),
            params.isa[0]->regClasses()),
    // Note: This references the CPU's regfile as it's a hardcoded map to copy in on restore
    freeList(name() + ".freelist", &cpu->regFile),
    scoreboard(name() + ".freelist", cpu->regFile.totalNumPhysRegs()),
    miscRegs(params.isa[0]->regClasses().at(MiscRegClass).numRegs())
{
    fatal_if(params.numThreads > 1, "Architectural state checkpointing is not supported with SMT\n");

    const BaseISA::RegClasses &regClasses = params.isa[0]->regClasses();

    // setup the list of starting flat indices for all register types
    typeFlatIndices[IntRegClass] = 0;
    for (int regTypeIdx = 1; regTypeIdx <= RegClassType::CCRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        RegClassType prevRegType = (RegClassType)(regType - 1);
        size_t prevRegTypeNumRegs = regClasses.at(prevRegType).numRegs();
        typeFlatIndices[regType] = typeFlatIndices[prevRegType] + prevRegTypeNumRegs;
    }

    // setup the checkpoint rename map. this should never change
    // note that, same as with the free list, this must init with the CPU regfile
    renameMap.init(regClasses, &cpu->regFile, &freeList);
    for (int regTypeIdx = 0; regTypeIdx <= RegClassType::CCRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        for (RegIndex idx = 0; idx < regClasses.at(regType).numRegs(); idx++) {
            RegId regId = RegId(regType, idx);
            PhysRegIdPtr phys_reg = freeList.getReg(regType);
            renameMap.setEntry(regId, phys_reg);
        }
    }
}

void
ArchCheckpoint::fullSave(ThreadID tid)
{
    assert(!cpu->inRunahead(tid));

    BaseISA *isa = cpu->getContext(tid)->getIsaPtr();
    const BaseISA::RegClasses &regClasses = isa->regClasses();

    RegIndex flatIdx = 0;
    // Copy all registers of all types defined by the ISA
    for (int regTypeIdx = 0; regTypeIdx <= RegClassType::MiscRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        const RegClass &cls = regClasses.at(regType);

        // for debug
        RegId dummyReg(regType, 0);
        DPRINTF(RunaheadCheckpoint, "[tid:%i] regtype %s has %i arch regs\n",
                tid, dummyReg.className(), cls.numRegs());

        for (RegIndex idx = 0; idx < cls.numRegs(); idx++) {
            RegId reg(regType, idx);
            PhysRegId physReg(regType, idx, flatIdx++);

            switch (regType) {
                case IntRegClass:
                    {
                        RegVal archVal = cpu->getArchReg(reg, tid);
                        DPRINTF(RunaheadCheckpoint, "[tid:%i] int reg: %s = %lld\n", tid, cls.regName(reg), archVal);
                        regFile.setReg(&physReg, archVal);
                    }
                    break;
                case FloatRegClass:
                    {
                        RegVal archVal = cpu->getArchReg(reg, tid);
                        DPRINTF(RunaheadCheckpoint, "[tid:%i] float reg: %s = %lld\n", tid, cls.regName(reg), archVal);
                        regFile.setReg(&physReg, archVal);
                    }
                    break;
                // case VecRegClass:
                //     {
                //         DPRINTF(RunaheadCheckpoint, "[tid:%i] vec reg: %s = ?", tid, cls.regName(reg), archVal);

                //         regFile.setReg(&physReg, ???);
                //     }
                case VecElemClass:
                    {
                        RegVal archVal = cpu->getArchReg(reg, tid);
                        DPRINTF(RunaheadCheckpoint, "[tid:%i] vec elem reg: %s = %lld\n", tid, cls.regName(reg), archVal);
                        regFile.setReg(&physReg, archVal);
                    }
                    break;
                // case VecPredRegClass:
                //     {
                //         DPRINTF(RunaheadCheckpoint, "[tid:%i] vec pred reg: %s = ?", tid, cls.regName(reg), archVal);

                //         regFile.setReg(&physReg, ???);
                //     }
                case CCRegClass:
                    {
                        RegVal archVal = cpu->getArchReg(reg, tid);
                        DPRINTF(RunaheadCheckpoint, "[tid:%i] cc reg: %s = %lld\n", tid, cls.regName(reg), archVal);
                        regFile.setReg(&physReg, archVal);
                    }
                    break;
                case MiscRegClass:
                    {
                        // misc registers are a pain because they are managed by the ISA, not the CPU
                        // x86 specific: it has invalid misc registers
                        if (!TheISA::misc_reg::isValid(idx)) {
                            break;
                        }

                        RegVal archVal = cpu->readMiscReg(idx, tid);
                        DPRINTF(RunaheadCheckpoint, "[tid:%i] misc reg: %s = %lld\n", tid, cls.regName(reg), archVal);
                        miscRegs[idx] = archVal;
                    }
                    break;
                default:
                    DPRINTF(RunaheadCheckpoint, "[tid:%i] unknown/unsupported regtype %i [%i], not checkpointing\n", tid, regType, idx);
                    break;
            }
        }
    }
}

void
ArchCheckpoint::restore(ThreadID tid)
{
    BaseISA *isa = cpu->getContext(tid)->getIsaPtr();
    const BaseISA::RegClasses &regClasses = isa->regClasses();

    // Copy over the rename map to both the rename stage map and the commit map
    cpu->copyRenameMap(tid, renameMap);
    cpu->copyCommitRenameMap(tid, renameMap);

    // Copy the hardcoded free list to the CPU's free list
    cpu->copyFreeList(freeList);
    // And the scoreboard
    cpu->copyScoreboard(scoreboard);

    // Then copy the architectural register values into the physical regs specified by the rename map
    for (int regTypeIdx = 0; regTypeIdx <= RegClassType::MiscRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        const RegClass &cls = regClasses.at(regType);

        // unsupported
        if (regType == VecRegClass || regType == VecPredRegClass) {
            continue;
        }

        for (RegIndex idx = 0; idx < cls.numRegs(); idx++) {
            RegId reg(regType, idx);
            PhysRegIdPtr checkpointPhysReg = renameMap.lookup(reg);

            if (regType == MiscRegClass) {
                // x86 specific: it has invalid misc registers
                if (!TheISA::misc_reg::isValid(idx)) {
                    continue;
                }

                RegVal curVal = cpu->readMiscReg(idx, tid);
                if (curVal != miscRegs[idx] && idx == TheISA::misc_reg::Rflags) {
                    DPRINTF(RunaheadCheckpoint, "[tid:%i] Restoring misc reg %i to value %llu (was %llu)\n",
                            tid, idx, miscRegs[idx], curVal);
                    cpu->setMiscReg(idx, miscRegs[idx], tid);
                }
            } else {
                RegVal checkpointVal = regFile.getReg(checkpointPhysReg);
                RegVal curVal = cpu->getArchReg(reg, tid);
                // Check if the value is actually different.
                // This is mostly just to reduce the amount of debug prints
                // Since this is done after the rename map copy it's pretty much just chance if they're the same
                if (curVal != checkpointVal) {
                    DPRINTF(RunaheadCheckpoint,
                        "[tid:%i] Restoring %s arch reg %i (phys %i) to value %llu (was %llu)\n",
                        tid, reg.className(), reg.index(),
                        checkpointPhysReg->index(), checkpointVal, curVal);
                    cpu->setArchReg(reg, checkpointVal, tid);
                }
            }
        }
    }
}

void
ArchCheckpoint::updateReg(ThreadID tid, RegId archReg)
{
    // Checkpoint updates are disallowed while in runahead
    assert(!cpu->inRunahead(tid));

    if (archReg.classValue() == VecRegClass || archReg.classValue() == VecPredRegClass) {
        DPRINTF(RunaheadCheckpoint, "VecRegClass/VecPredClass register checkpointing is unsupported. This update is ignored.\n");
        return;
    }

    if (archReg.classValue() == InvalidRegClass)
        return;

    RegVal val;
    if (archReg.classValue() == MiscRegClass) {
        RegIndex archIdx = archReg.index();
        val = cpu->readMiscReg(archIdx, tid);
        miscRegs[archIdx] = val;
    } else {
        PhysRegIdPtr physReg = renameMap.lookup(archReg);
        val = cpu->getArchReg(archReg, tid);
        regFile.setReg(physReg, val);
    }
}

} // namespace runahead
} // namespace gem5
