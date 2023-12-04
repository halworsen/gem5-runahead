#ifndef __CPU_RUNAHEAD_PC_PAIR_HH__
#define __CPU_RUNAHEAD_PC_PAIR_HH__

#include "arch/generic/pcstate.hh"

namespace gem5
{

namespace runahead
{

struct PCPair
{
public:
    Addr pc = 0;
    MicroPC upc = 0;

    PCPair(const PCStateBase &pcState)
    {
        pc = pcState.instAddr();
        upc = pcState.microPC();
    }

    bool
    operator==(PCPair other)
    {
        return (pc == other.pc && upc == other.upc);
    }

    bool
    operator==(const PCStateBase &pcb)
    {
        return (pc == pcb.instAddr() && upc == pcb.microPC());
    }
};

typedef std::vector<PCPair>::iterator ChainIt;

} // namespace runahead
} // namespace gem5

#endif // __CPU_RUNAHEAD_PC_PAIR_HH__
