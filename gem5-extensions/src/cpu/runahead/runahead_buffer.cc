#include <vector>

#include "cpu/runahead/runahead_buffer.hh"
#include "cpu/runahead/dyn_inst.hh"
#include "cpu/runahead/lsq_unit.hh"

namespace gem5
{
namespace runahead
{

Cycles
RunaheadBuffer::generateDependenceChain(DynInstPtr inst)
{
    ThreadID tid = inst->threadNumber;
    Addr instPC = inst->pcState().instAddr();

    assert(rob->readHeadInst(tid) == inst);

    // First pass, try to find an inst with the same PC (i.e. if the chain can be generated at all)
    auto effectiveEnd = rob->end(tid);
    bool canGenerateChain = false;
    for (auto i = ++rob->begin(tid); i != rob->end(tid); i++) {
        DynInstPtr robInst = *i;

        if (robInst->pcState().instAddr() == instPC) {
            effectiveEnd = i;
            canGenerateChain = true;
        }
    }

    if (!canGenerateChain)
        return Cycles(0);

    // Setup the source register search list (SRSL) and dependence chain
    SRSL srsl;
    DepChain chain;

    // Add inst to dependence chain and enqueue all source regs
    std::unique_ptr<PCStateBase> pc(inst->pcState().clone());
    chain.emplace_back(tid, inst->staticInst, inst->macroop, pc);
    for (int i = 0; i < inst->numSrcRegs(); i++) {
        PhysRegIdPtr physReg = inst->renamedSrcIdx(i);
        srsl.push(physReg);
    }

    // Generate dep chain by iteratively searching the ROB for register producers
    // Use effectiveEnd to search between the head and the oldest re-occurence of the inst in the ROB
    iterativeDCGen(tid, effectiveEnd, chain, srsl);
    stats.chainLength.sample(chain.size());

    // Should have taken as many cycles as there are insts in the DC,
    // excluding the first "initialization" cycle
    int cycles = (chain.size() - 1);
    return Cycles(cycles);
}

void
RunaheadBuffer::iterativeDCGen(ThreadID tid, ROBIt effectiveEnd, DepChain &chain, SRSL &srsl)
{
    while (!srsl.empty() && chain.size() < maxDCLength) {
        PhysRegIdPtr curSrcReg = srsl.front();

        // ++ to skip the inst we're doing the DC generation for
        bool producerFound = false;
        for (auto i = ++rob->begin(tid); i != effectiveEnd; i++) {
            DynInstPtr robInst = *i;
            if (robInst->isControl())
                continue;

            // Check if the inst produces the source reg from the SRSL
            for (int j = 0; j < robInst->numDestRegs(); j++) {
                if (robInst->renamedDestIdx(j) == curSrcReg) {
                    std::unique_ptr<PCStateBase> pc(robInst->pcState().clone());
                    chain.emplace_back(
                        robInst->threadNumber,
                        robInst->staticInst,
                        robInst->macroop,
                        pc
                    );

                    // Add all of its source registers to the SRSL
                    for (int k = 0; k < robInst->numSrcRegs(); k++) {
                        PhysRegIdPtr physReg = robInst->renamedSrcIdx(k);
                        srsl.push(physReg);
                    }

                    // If it was a load, search the SQ by load address for any stores
                    // producing the address, so we can add those stores to the chain as well
                    if (robInst->isLoad() && robInst->effAddrValid())
                        searchForMemProducers(tid, robInst->effAddr, chain, srsl);

                    producerFound = true;
                    break;
                }
            }

            if (producerFound)
                break;
        }

        srsl.pop();
    }
}

void
RunaheadBuffer::searchForMemProducers(ThreadID tid, Addr addr, DepChain &chain, SRSL &srsl)
{
    const LSQUnit &lsqUnit = lsq->getUnit(tid);
    LSQUnit::StoreQueue sq = lsqUnit.storeQueue;


}

RunaheadBuffer::RunaheadBufferStats::RunaheadBufferStats(statistics::Group *parent)
    : statistics::Group(parent, "runaheadbuffer"),
      ADD_STAT(chainLength, statistics::units::Count::get(),
           "Lengths of dependence chains")
{
    chainLength
        .init(0, maxDCLength, 8)
        .flags(statistics::total);
}

} // namespace runahead
} // namespace gem5