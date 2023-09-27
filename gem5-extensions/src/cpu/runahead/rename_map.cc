/*
 * Copyright (c) 2016-2018,2019 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder. You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "cpu/runahead/rename_map.hh"

#include <vector>

#include "arch/vecregs.hh"
#include "cpu/runahead/dyn_inst.hh"
#include "cpu/reg_class.hh"
#include "debug/Rename.hh"

namespace gem5
{

namespace runahead
{

SimpleRenameMap::SimpleRenameMap() : freeList(NULL)
{
}


void
SimpleRenameMap::init(const RegClass &reg_class, SimpleFreeList *_freeList)
{
    assert(freeList == NULL);
    assert(map.empty());

    map.resize(reg_class.numRegs());
    freeList = _freeList;
}

SimpleRenameMap::RenameInfo
SimpleRenameMap::rename(const RegId& arch_reg)
{
    PhysRegIdPtr renamed_reg;
    // Record the current physical register that is renamed to the
    // requested architected register.
    PhysRegIdPtr prev_reg = map[arch_reg.index()];

    if (arch_reg.is(InvalidRegClass)) {
        assert(prev_reg->is(InvalidRegClass));
        renamed_reg = prev_reg;
    } else if (prev_reg->getNumPinnedWrites() > 0) {
        // Do not rename if the register is pinned
        assert(arch_reg.getNumPinnedWrites() == 0);  // Prevent pinning the
                                                     // same register twice
        DPRINTF(Rename, "Renaming pinned reg, numPinnedWrites %d\n",
                prev_reg->getNumPinnedWrites());
        renamed_reg = prev_reg;
        renamed_reg->decrNumPinnedWrites();
    } else {
        renamed_reg = freeList->getReg();
        map[arch_reg.index()] = renamed_reg;
        renamed_reg->setNumPinnedWrites(arch_reg.getNumPinnedWrites());
        renamed_reg->setNumPinnedWritesToComplete(
            arch_reg.getNumPinnedWrites() + 1);
    }

    DPRINTF(Rename, "Renamed reg %d to physical reg %d (flat %d) old mapping was"
            " %d (flat %d)\n",
            arch_reg, renamed_reg->index(), renamed_reg->flatIndex(),
            prev_reg->index(), prev_reg->flatIndex());

    return RenameInfo(renamed_reg, prev_reg);
}

void
SimpleRenameMap::reset(size_t numRegs)
{
    map.clear();
    map.resize(numRegs);
}

void
SimpleRenameMap::dump()
{
    for (int archIdx = 0; archIdx < map.size(); archIdx++) {
        cprintf("%i -> %i, ", archIdx, map[archIdx]->index());
        if (!(archIdx % 10))
            cprintf("\n");
    }
    cprintf("\n");
}

/**** UnifiedRenameMap methods ****/

void
UnifiedRenameMap::init(const BaseISA::RegClasses &regClasses,
        PhysRegFile *_regFile, UnifiedFreeList *freeList)
{
    regFile = _regFile;

    for (int i = 0; i < renameMaps.size(); i++)
        renameMaps[i].init(regClasses.at(i), &(freeList->freeLists[i]));
}

bool
UnifiedRenameMap::canRename(DynInstPtr inst) const
{
    for (int i = 0; i < renameMaps.size(); i++) {
        if (inst->numDestRegs((RegClassType)i) >
                renameMaps[i].numFreeEntries()) {
            return false;
        }
    }
    return true;
}

void
UnifiedRenameMap::setFreeList(UnifiedFreeList *freeList)
{
    for (int i = 0; i < renameMaps.size(); i++)
        renameMaps[i].setFreeList(&(freeList->freeLists[i]));
}

void
UnifiedRenameMap::dump()
{
    for (int typeIdx = 0; typeIdx < renameMaps.size(); typeIdx++) {
        RegId dummyReg((RegClassType)typeIdx, 0);
        cprintf("Rename map for reg type %s (arch -> phys):\n", dummyReg.className());
        renameMaps[typeIdx].dump();
    }
}

void
UnifiedRenameMap::reset(const BaseISA::RegClasses &regClasses)
{
    for (int typeIdx = 0; typeIdx < renameMaps.size(); typeIdx++) {
        RegClassType regType = static_cast<RegClassType>(typeIdx);
        size_t numRegs = regClasses.at(regType).numRegs();

        // Reset the individual rename maps (clear them)
        renameMaps[regType].reset(numRegs);
    }
}

} // namespace runahead
} // namespace gem5
