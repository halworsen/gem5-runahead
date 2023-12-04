#ifndef __CPU_RUNAHEAD_PC_DEFS_HH__
#define __CPU_RUNAHEAD_PC_DEFS_HH__

#include <vector>
#include <memory>

#include "arch/generic/pcstate.hh"

namespace gem5
{

namespace runahead
{

typedef std::unique_ptr<PCStateBase> PCStatePtr;

} // namespace runahead
} // namespace gem5

#endif // __CPU_RUNAHEAD_PC_DEFS_HH__
