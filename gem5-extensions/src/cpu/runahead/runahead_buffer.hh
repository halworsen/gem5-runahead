#ifndef _CPU_RUNAHEAD_RUNAHEAD_BUFFER_HH__
#define _CPU_RUNAHEAD_RUNAHEAD_BUFFER_HH__

#include <vector>
#include <queue>

#include "base/statistics.hh"
#include "base/types.hh"
#include "cpu/runahead/comm.hh"
#include "cpu/timebuf.hh"
#include "cpu/reg_class.hh"
#include "cpu/runahead/rob.hh"
#include "cpu/runahead/dyn_inst_ptr.hh"

namespace gem5
{
namespace runahead
{

/**
 * The runahead buffer replaces fetch/decode, i.e. the processor frontend, while the processor
 * is in runahead mode. This is the same structure as in Hashemi and Patt's
 * "Filtered Runahead Execution with a Runahead Buffer" (2015).
 * 
 * It contains slices of code that generate addresses for long latency loads. When the processor
 * is in runahead mode, fetch and decode are de-activated. Instead, the runahead buffer is
 * activated to supply instructions to rename.
 * 
 * Instructions are supplied from the dependence chain corresponding to the load that caused
 * entry into runahead. Once the chain has finished executing (i.e. the final load is sent to rename),
 * we start over at the first instruction of the dependence chain, thereby executing in a loop.
*/
class RunaheadBuffer
{
private:
    /** Contains all info necessary to generate a dynamic inst */
    struct DepChainEntry
    {
        ThreadID tid;
        StaticInstPtr staticInst;
        StaticInstPtr macroOp;
        std::unique_ptr<PCStateBase> pc;
    };
    typedef std::vector<DepChainEntry> DepChain;
    typedef std::queue<PhysRegIdPtr> SRSL;
    typedef std::list<DynInstPtr>::iterator ROBIt;

    /**
     * Maximum dependence chain length, in amount of instructions.
     * TODO: parametrize
    */
    static const size_t maxDCLength = 32;

    /** Pointer to the CPU */
    CPU *cpu;

    /** Pointer to the ROB */
    ROB *rob;

    /** Pointer to the LSQ */
    LSQ *lsq;

    /** Wire used to write instructions to rename. */
    TimeBuffer<DecodeStruct>::wire toRename;

public:
    /**
     * Generate the dependence chain of the given instruction using the instructions in the ROB.
     * 
     * The return value is the amount of instructions in the dependence chain, corresponding to
     * how long it would take to generate the chain by iteratively searching the ROB (1 inst/cycle).
    */
    Cycles generateDependenceChain(DynInstPtr inst);

private:
    void iterativeDCGen(ThreadID tid, ROBIt effectiveEnd, DepChain &chain, SRSL &srsl);

    void searchForMemProducers(ThreadID tid, Addr addr, DepChain &chain, SRSL &srsl);

    struct RunaheadBufferStats : public statistics::Group
    {
        RunaheadBufferStats(statistics::Group *parent);

        /** Dependence chain lengths */
        statistics::Distribution chainLength;
    } stats;
}

} // namespace runahead
} // namespace gem5