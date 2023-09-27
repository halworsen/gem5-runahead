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
    cpu(cpu), numThreads(params.numThreads)
{
    const auto &regClasses = params.isa[0]->regClasses();

    // setup checkpoint list
    // TODO: vector register/pred register support. ISA regclasses store their reg size
    for (int regTypeIdx = 0; regTypeIdx <= MiscRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        const RegClass &cls = regClasses.at(regType);

        RegCheckpoint &checkpoint = registerCheckpoints[regTypeIdx];
        checkpoint.values.resize(cls.numRegs());
    }
}

void
ArchCheckpoint::fullSave(ThreadID tid)
{
    // Save all the architectural registers
    const auto &regClasses = cpu->isa[0]->regClasses();
    for (int regTypeIdx = 0; regTypeIdx < registerCheckpoints.size(); regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        if (regType == VecRegClass || regType == VecPredRegClass)
            continue;

        RegClass regClass = regClasses.at(regType);
        size_t numRegs = regClass.numRegs();

        RegCheckpoint &checkpoint = registerCheckpoints[regTypeIdx];
        checkpoint.validIdxs.clear();
        for (RegIndex archIdx = 0; archIdx < numRegs; archIdx++) {
            RegId archReg(regType, archIdx);
            RegVal val;
            if (regType == MiscRegClass) {
                // x86 specific
                if (!TheISA::misc_reg::isValid(archIdx))
                    continue;
                val = cpu->readMiscReg(archIdx, tid);
            } else {
                val = cpu->getArchReg(archReg, tid);
            }

            checkpoint.values[archIdx] = val;
            checkpoint.validIdxs.push_back(archIdx);
        }
    }
}

void
ArchCheckpoint::restore(ThreadID tid)
{
    // Go through all the checkpoints to restore
    for (int regTypeIdx = 0; regTypeIdx < registerCheckpoints.size(); regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        // unsupported
        if (regType == VecRegClass || regType == VecPredRegClass || regType == MiscRegClass)
            continue;

        // Go through all value checkpoints for this specific register type, e.g. all int reg checkpoints
        RegCheckpoint &checkpoint = registerCheckpoints[regTypeIdx];
        if (checkpoint.validIdxs.empty())
            continue;

        for (auto i = checkpoint.validIdxs.begin(); i != checkpoint.validIdxs.end(); i++) {
            RegIndex archIdx = *i;
            RegId reg(regType, archIdx);
            RegVal curVal;
            RegVal checkpointVal = checkpoint.values[archIdx];

            if (regType != MiscRegClass) {
                curVal = cpu->getArchReg(reg, tid);

                // Check if the value is actually different.
                // This is mostly just to reduce the amount of debug prints
                if (curVal != checkpointVal) {
                    DPRINTF(RunaheadCheckpoint,
                        "[tid:%i] Restoring %s arch reg %i to value %s (was %s)\n",
                        tid, reg.className(), reg.index(),
                        checkpointVal, curVal);
                    cpu->setArchReg(reg, checkpointVal, tid);
                }
            } else {
                curVal = cpu->readMiscReg(archIdx, tid);
                if (curVal != checkpointVal) { // && archIdx == TheISA::misc_reg::Rflags
                    DPRINTF(RunaheadCheckpoint, "[tid:%i] Restoring misc reg %i to value %llu (was %llu)\n",
                            tid, archIdx, checkpointVal, curVal);
                    cpu->setMiscReg(archIdx, checkpointVal, tid);
                }
            }
        }

        // All checkpoints for this register class have been restored, make them invalid
        checkpoint.validIdxs.clear();
    }
}

void
ArchCheckpoint::updateReg(ThreadID tid, RegId archReg)
{
    // Checkpoint updates should not happen in runahead
    assert(!cpu->inRunahead(tid));

    if (archReg.classValue() == VecRegClass || archReg.classValue() == VecPredRegClass) {
        DPRINTF(RunaheadCheckpoint, "VecRegClass/VecPredClass register checkpointing is unsupported. This update is ignored.\n");
        return;
    }

    if (archReg.classValue() == InvalidRegClass)
        return;

    RegVal val;
    RegIndex archIdx = archReg.index();
    RegClassType regClass = archReg.classValue();
    if (archReg.classValue() == MiscRegClass) {
        // x86 specific: it has invalid misc registers
        if (!TheISA::misc_reg::isValid(archIdx))
            return;
        val = cpu->readMiscReg(archIdx, tid);
    } else {
        val = cpu->getArchReg(archReg, tid);
    }

    RegCheckpoint &checkpoint = registerCheckpoints[regClass];
    checkpoint.values[archIdx] = val;

    // Admittedly this is a ugly hack implementation of a set of unique indices
    // Would've used std::set or std::unordered_set but they segfault and I don't know why
    bool inValidSet = false;
    for (auto i = checkpoint.validIdxs.begin(); i != checkpoint.validIdxs.end(); i++) {
        RegIndex idx = *i;
        if (idx == archIdx) {
            inValidSet = true;
            break;
        }
    }

    if (!inValidSet)
        checkpoint.validIdxs.push_back(archIdx);
}

} // namespace runahead
} // namespace gem5
