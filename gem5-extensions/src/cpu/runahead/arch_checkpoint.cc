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
    miscRegValues(params.isa[0]->regClasses().at(MiscRegClass).numRegs())
{
    fatal_if(params.numThreads > 1, "Architectural state checkpointing is not supported with SMT\n");

    // setup the list of starting flat indices for all register types
    typeFlatIndices[IntRegClass] = 0;
    for (int regTypeIdx = 1; regTypeIdx <= RegClassType::CCRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        RegClassType prevRegType = (RegClassType)(regType - 1);
        size_t prevRegTypeNumRegs = params.isa[0]->regClasses().at(prevRegType).numRegs();
        typeFlatIndices[regType] = typeFlatIndices[prevRegType] + prevRegTypeNumRegs;
    }
}

void
ArchCheckpoint::fullSave(ThreadID tid)
{
    assert(!cpu->inRunahead(tid));

    checkpointPc = cpu->pcState(tid).instAddr();
    DPRINTF(RunaheadCheckpoint, "[tid:%i] Saving architectural state at PC: %#x\n", tid, checkpointPc);

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
                        miscRegValues[idx] = archVal;
                    }
                    break;
                default:
                    DPRINTF(RunaheadCheckpoint, "[tid:%i] unknown/ignored regtype %i [%i], not checkpointing\n", tid, regType, idx);
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

    RegIndex flatIdx = 0;
    for (int regTypeIdx = 0; regTypeIdx <= RegClassType::MiscRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        const RegClass &cls = regClasses.at(regType);

        // unsupported
        if (regType == VecRegClass || regType == VecPredRegClass) {
            continue;
        }

        for (RegIndex idx = 0; idx < cls.numRegs(); idx++) {
            RegId reg(regType, idx);
            PhysRegId checkpointPhysReg(regType, idx, flatIdx++);
            
            if (regType == MiscRegClass) {
                // x86 specific: it has invalid misc registers
                if (!TheISA::misc_reg::isValid(idx)) {
                    continue;
                }
                cpu->setMiscReg(idx, miscRegValues[idx], tid);
            } else {
                cpu->setArchReg(reg, regFile.getReg(&checkpointPhysReg), tid);
            }
        }
    }
}

void
ArchCheckpoint::setPC(ThreadID tid, Addr pcAddr)
{
    checkpointPc = pcAddr;
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

    if (archReg.classValue() == InvalidRegClass) {
        DPRINTF(RunaheadCheckpoint, "Attempt to checkpoint invalid register\n");
        return;
    }

    RegVal val;
    if (archReg.classValue() == MiscRegClass) {
        val = cpu->readMiscReg(archReg.index(), tid);
        miscRegValues[archReg.index()] = val;
    } else {
        RegId archRegId(archReg.classValue(), archReg);
        val = cpu->getArchReg(archRegId, tid);

        PhysRegId physArchRegId(
            archReg.classValue(),
            archReg.index(),
            typeFlatIndices[archReg.classValue()] + archReg.index()
        );
        regFile.setReg(&physArchRegId, val);
    }
}

} // namespace runahead
} // namespace gem5
