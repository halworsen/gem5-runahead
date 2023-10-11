#include "arch/x86/regs/misc.hh"

namespace gem5
{
namespace X86ISA
{
namespace misc_reg
{

/** Array of correctness critical misc registers */
const RegIndex execCriticalRegs[] = {
    Rflags
};
static const size_t numCriticalRegs = sizeof(execCriticalRegs) / sizeof(execCriticalRegs[0]);

/** Check if the given misc. register is critical for correct execution */
bool isRegCritical(RegIndex reg);

} // namespace gem5
} // namespace X86ISA
} // namespace misc_reg
