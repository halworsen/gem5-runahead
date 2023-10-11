#include "misc_ext.hh"

namespace gem5
{
namespace X86ISA
{
namespace misc_reg
{

bool
isRegCritical(RegIndex reg)
{
    for (size_t i = 0; i < numCriticalRegs; i++) {
        if (reg == execCriticalRegs[i])
            return true;
    }

    return false;
}

} // namespace gem5
} // namespace X86ISA
} // namespace misc_reg