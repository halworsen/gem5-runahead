/*
 * Copyright (c) 2012 ARM Limited
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Copyright (c) 2004-2006 The Regents of The University of Michigan
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

#include "cpu/runahead/rob.hh"

#include <list>
#include <algorithm>

#include "base/logging.hh"
#include "cpu/runahead/dyn_inst.hh"
#include "cpu/runahead/limits.hh"
#include "debug/Fetch.hh"
#include "debug/ROB.hh"
#include "debug/RunaheadROB.hh"
#include "debug/RunaheadChains.hh"
#include "params/BaseRunaheadCPU.hh"

namespace gem5
{

namespace runahead
{

ROB::ROB(CPU *_cpu, const BaseRunaheadCPUParams &params)
    : robPolicy(params.smtROBPolicy),
      cpu(_cpu),
      numEntries(params.numROBEntries),
      squashWidth(params.squashWidth),
      numInstsInROB(0),
      numThreads(params.numThreads),
      stats(_cpu)
{
    //Figure out rob policy
    if (robPolicy == SMTQueuePolicy::Dynamic) {
        //Set Max Entries to Total ROB Capacity
        for (ThreadID tid = 0; tid < numThreads; tid++) {
            maxEntries[tid] = numEntries;
        }

    } else if (robPolicy == SMTQueuePolicy::Partitioned) {
        DPRINTF(Fetch, "ROB sharing policy set to Partitioned\n");

        //@todo:make work if part_amt doesnt divide evenly.
        int part_amt = numEntries / numThreads;

        //Divide ROB up evenly
        for (ThreadID tid = 0; tid < numThreads; tid++) {
            maxEntries[tid] = part_amt;
        }

    } else if (robPolicy == SMTQueuePolicy::Threshold) {
        DPRINTF(Fetch, "ROB sharing policy set to Threshold\n");

        int threshold =  params.smtROBThreshold;;

        //Divide up by threshold amount
        for (ThreadID tid = 0; tid < numThreads; tid++) {
            maxEntries[tid] = threshold;
        }
    }

    for (ThreadID tid = numThreads; tid < MaxThreads; tid++) {
        maxEntries[tid] = 0;
    }

    resetState();
}

void
ROB::resetState()
{
    for (ThreadID tid = 0; tid  < MaxThreads; tid++) {
        threadEntries[tid] = 0;
        squashIt[tid] = instList[tid].end();
        squashedSeqNum[tid] = 0;
        doneSquashing[tid] = true;
    }
    numInstsInROB = 0;

    // Initialize the "universal" ROB head & tail point to invalid
    // pointers
    head = instList[0].end();
    tail = instList[0].end();
}

std::string
ROB::name() const
{
    return cpu->name() + ".rob";
}

void
ROB::setActiveThreads(std::list<ThreadID> *at_ptr)
{
    DPRINTF(ROB, "Setting active threads list pointer.\n");
    activeThreads = at_ptr;
}

void
ROB::drainSanityCheck() const
{
    for (ThreadID tid = 0; tid  < numThreads; tid++)
        assert(instList[tid].empty());
    assert(isEmpty());
}

void
ROB::archRestoreSanityCheck(ThreadID tid)
{
    bool allSquashed = true;
    for (DynInstPtr inst : instList[tid]) {
        if (!inst->isSquashed()) {
            allSquashed = false;
            break;
        }
    }
    assert(!isDoneSquashing() || isEmpty() || allSquashed);
}

void
ROB::takeOverFrom()
{
    resetState();
}

void
ROB::resetEntries()
{
    if (robPolicy != SMTQueuePolicy::Dynamic || numThreads > 1) {
        auto active_threads = activeThreads->size();

        std::list<ThreadID>::iterator threads = activeThreads->begin();
        std::list<ThreadID>::iterator end = activeThreads->end();

        while (threads != end) {
            ThreadID tid = *threads++;

            if (robPolicy == SMTQueuePolicy::Partitioned) {
                maxEntries[tid] = numEntries / active_threads;
            } else if (robPolicy == SMTQueuePolicy::Threshold &&
                       active_threads == 1) {
                maxEntries[tid] = numEntries;
            }
        }
    }
}

int
ROB::entryAmount(ThreadID num_threads)
{
    if (robPolicy == SMTQueuePolicy::Partitioned) {
        return numEntries / num_threads;
    } else {
        return 0;
    }
}

int
ROB::countInsts()
{
    int total = 0;

    for (ThreadID tid = 0; tid < numThreads; tid++)
        total += countInsts(tid);

    return total;
}

size_t
ROB::countInsts(ThreadID tid)
{
    return instList[tid].size();
}

void
ROB::generateChainBuffer(const DynInstPtr &inst, std::vector<PCStatePtr> &buffer)
{
    DPRINTF(RunaheadROB, "Attempting to generate dependence chain for sn:%llu\n",
            inst->seqNum);
    ThreadID tid = inst->threadNumber;
    InstIt instPos = std::find(instList[tid].begin(), instList[tid].end(), inst);
    if (instPos == instList[tid].end())
        return;

    // Try to find a younger copy of the inst in the ROB, starting at the inst directly after this one
    // Without this, we cannot generate the chain immediately as the chain is not in the ROB
    instPos++;
    InstIt youngerPos = instList[tid].end();
    for (InstIt it = instPos; it != instList[tid].end(); it++) {
        if ((*it)->pcState() == inst->pcState()) {
            youngerPos = it;
            break;
        }
    }

    // No younger copy, can't construct the chain
    if (youngerPos == instList[tid].end()) {
        DPRINTF(RunaheadROB, "Unable to find younger instance of inst. No chain generated.\n");
        return;
    } else {
        DPRINTF(RunaheadROB, "Younger instance of inst found with sn:%llu\n",
                (*youngerPos)->seqNum);
    }

    // For debug/analysis with the RunaheadChains debug flag
    std::vector<std::string> _instChain;

    // Source Register Search List
    struct SRSLEntry {
        PhysRegIdPtr srcReg;
        // The ROB position to start looking for producers at
        InstIt startIt;
        SRSLEntry(PhysRegIdPtr reg, InstIt it) : srcReg(reg), startIt(it) {}
    };
    std::queue<SRSLEntry> srsl;

    // Add the younger inst to the chain
    buffer.emplace_back((*youngerPos)->pcState().clone());
    _instChain.push_back((*youngerPos)->staticInst->disassemble((*youngerPos)->pcState().instAddr()));
    DPRINTF(RunaheadROB, "Adding sn:%llu to dependence chain (size: %i): %s\n",
            (*youngerPos)->seqNum, buffer.size(),
            (*youngerPos)->staticInst->disassemble((*youngerPos)->pcState().instAddr()));
    // Add the younger inst's physical source registers to the SRSL
    for (int i = 0; i < (*youngerPos)->numSrcRegs(); i++) {
        PhysRegIdPtr reg = (*youngerPos)->renamedSrcIdx(i);
        if (reg->classValue() == InvalidRegClass || reg->classValue() == MiscRegClass)
            continue;
        InstIt startIt = youngerPos;
        startIt--;
        srsl.emplace(reg, startIt);
        DPRINTF(RunaheadROB, "Adding %s %i after sn:%llu to SRSL\n",
                reg->className(), reg->index(),
                (*startIt)->seqNum);
    }

    // Start constructing the dependence chain
    while (!srsl.empty()) {
        // Pop a source reg to look for in the ROB
        SRSLEntry entry = srsl.front();
        PhysRegIdPtr searchSrcReg = entry.srcReg;
        InstIt startPos = entry.startIt;
        srsl.pop();

        DPRINTF(RunaheadROB, "SRSL size: %i. Attempting to find producers for %s %i, starting at sn:%llu...\n",
                srsl.size(), searchSrcReg->className(), searchSrcReg->index(),
                (*startPos)->seqNum);

        // Then go from the start position towards the oldest instance looking for producers
        for (InstIt it = startPos; it != instPos; it--) {
            DynInstPtr inst = *it;

            // Check if this inst produces the register
            bool isProducer = false;
            for (int i = 0; i < inst->numDestRegs(); i++) {
                if (*inst->renamedDestIdx(i) == *searchSrcReg) {
                    isProducer = true;
                    break;
                }
            }

            if (isProducer) {
                DPRINTF(RunaheadROB, "sn:%llu is a producer!\n", inst->seqNum);

                // If it was a producer, add it to the chain
                bool inChain = std::find_if(
                    buffer.begin(), buffer.end(),
                    [inst](PCStatePtr &pc) { return pc->equals(inst->pcState()); }
                ) == buffer.end();
               if (inChain) {
                    buffer.emplace_back(inst->pcState().clone());
                    _instChain.push_back(inst->staticInst->disassemble(inst->pcState().instAddr()));
                    DPRINTF(RunaheadROB, "Adding sn:%llu to dependence chain (size: %i): %s\n",
                            inst->seqNum, buffer.size(),
                            inst->staticInst->disassemble(inst->pcState().instAddr()));
                } else {
                    DPRINTF(RunaheadROB, "Inst was already in the chain, ignoring.\n");
                    break;
                }

                // Then add its source regs to the SRSL
                for (int i = 0; i < inst->numSrcRegs(); i++) {
                    PhysRegIdPtr reg = inst->renamedSrcIdx(i);
                    if (reg->classValue() == InvalidRegClass || reg->classValue() == MiscRegClass)
                        continue;
                    InstIt startIt = it;
                    startIt--;
                    srsl.emplace(reg, startIt);
                    DPRINTF(RunaheadROB, "Adding %s %i after sn:%llu to SRSL\n",
                            reg->className(), reg->index(),
                            (*startIt)->seqNum);
                }

                // For loads: check the SQ for matching addresses
                if (!inst->isLoad())
                    break;

                DPRINTF(RunaheadROB, "Inst was a load, searching SQ for overlapping stores.\n");
                if (!cpu->hasOverlappingStore(inst))
                    break;

                // If there was one, add it to the chain and all of its regs to the SRSL
                const DynInstPtr &prodStore = cpu->getOverlappingStore(inst);
                DPRINTF(RunaheadROB, "sn:%llu is an overlapping store!\n", prodStore->seqNum);
                for (int i = 0; i < prodStore->numSrcRegs(); i++) {
                    PhysRegIdPtr reg = prodStore->renamedSrcIdx(i);
                    if (reg->classValue() == InvalidRegClass || reg->classValue() == MiscRegClass)
                        continue;
                    InstIt startIt = std::find(instList[tid].begin(), instList[tid].end(), prodStore);
                    startIt--;
                    srsl.emplace(reg, startIt);
                    DPRINTF(RunaheadROB, "Adding %s %i after sn:%llu to SRSL\n",
                            reg->className(), reg->index(),
                            (*startIt)->seqNum);
                }

                inChain = std::find_if(
                    buffer.begin(), buffer.end(),
                    [inst](PCStatePtr &pc) { return pc->equals(inst->pcState()); }
                ) == buffer.end();
                if (inChain) {
                    buffer.emplace_back(prodStore->pcState().clone());
                    _instChain.push_back(prodStore->staticInst->disassemble(prodStore->pcState().instAddr()));
                    DPRINTF(RunaheadROB, "Adding sn:%llu to dependence chain (size: %i): %s\n",
                            prodStore->seqNum, buffer.size(),
                            prodStore->staticInst->disassemble(prodStore->pcState().instAddr()));
                } else {
                    DPRINTF(RunaheadROB, "Inst was already in the chain, ignoring.\n");
                }

                break;
            }
        }
    }

    // Reverse the order of the chain because we generated it back to front
    std::reverse(buffer.begin(), buffer.end());

    DPRINTF(RunaheadChains, "Final dependence chain size: %i insts\n",
            buffer.size());

    if (buffer.size()) {
        int i = 1;
        std::vector<std::string>::reverse_iterator it = _instChain.rbegin();
        for (; it != _instChain.rend(); it++) {
            DPRINTF(RunaheadChains, "Chain entry #%i: %s\n", i++, *it);
        }
    }
}

void
ROB::insertInst(const DynInstPtr &inst)
{
    assert(inst);

    stats.writes++;

    DPRINTF(ROB, "Adding inst PC %s to the ROB.\n", inst->pcState());

    assert(numInstsInROB != numEntries);

    ThreadID tid = inst->threadNumber;

    instList[tid].push_back(inst);

    //Set Up head iterator if this is the 1st instruction in the ROB
    if (numInstsInROB == 0) {
        head = instList[tid].begin();
        assert((*head) == inst);
    }

    //Must Decrement for iterator to actually be valid  since __.end()
    //actually points to 1 after the last inst
    tail = instList[tid].end();
    tail--;

    inst->setInROB();

    ++numInstsInROB;
    ++threadEntries[tid];

    assert((*tail) == inst);

    DPRINTF(ROB, "[tid:%i] Now has %d instructions.\n", tid,
            threadEntries[tid]);
}

void
ROB::retireHead(ThreadID tid)
{
    stats.writes++;

    assert(numInstsInROB > 0);

    // Get the head ROB instruction by copying it and remove it from the list
    InstIt head_it = instList[tid].begin();

    DynInstPtr head_inst = std::move(*head_it);
    instList[tid].erase(head_it);

    assert(head_inst->readyToCommit());

    DPRINTF(ROB, "[tid:%i] Retiring head instruction, "
            "instruction PC %s, [sn:%llu]\n", tid, head_inst->pcState(),
            head_inst->seqNum);

    --numInstsInROB;
    --threadEntries[tid];

    head_inst->clearInROB();
    head_inst->setCommitted();

    //Update "Global" Head of ROB
    updateHead();

    // @todo: A special case is needed if the instruction being
    // retired is the only instruction in the ROB; otherwise the tail
    // iterator will become invalidated.
    cpu->removeFrontInst(head_inst);
}

bool
ROB::isHeadReady(ThreadID tid)
{
    stats.reads++;
    if (threadEntries[tid] != 0) {
        DynInstPtr &head = instList[tid].front();
        return head->readyToCommit();
    }

    return false;
}

bool
ROB::canCommit()
{
    //@todo: set ActiveThreads through ROB or CPU
    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (isHeadReady(tid)) {
            return true;
        }
    }

    return false;
}

unsigned
ROB::numFreeEntries()
{
    return numEntries - numInstsInROB;
}

unsigned
ROB::numFreeEntries(ThreadID tid)
{
    return maxEntries[tid] - threadEntries[tid];
}

void
ROB::doSquash(ThreadID tid)
{
    stats.writes++;
    DPRINTF(ROB, "[tid:%i] Squashing instructions until [sn:%llu].\n",
            tid, squashedSeqNum[tid]);

    assert(squashIt[tid] != instList[tid].end());

    if ((*squashIt[tid])->seqNum < squashedSeqNum[tid]) {
        DPRINTF(ROB, "[tid:%i] Done squashing instructions.\n",
                tid);

        squashIt[tid] = instList[tid].end();

        doneSquashing[tid] = true;
        return;
    }

    bool robTailUpdate = false;

    unsigned int numInstsToSquash = squashWidth;

    // If the CPU is exiting, squash all of the instructions
    // it is told to, even if that exceeds the squashWidth.
    // Set the number to the number of entries (the max).
    if (cpu->isThreadExiting(tid))
    {
        numInstsToSquash = numEntries;
    }

    for (int numSquashed = 0;
         numSquashed < numInstsToSquash &&
         squashIt[tid] != instList[tid].end() &&
         (*squashIt[tid])->seqNum > squashedSeqNum[tid];
         ++numSquashed)
    {
        DPRINTF(ROB, "[tid:%i] Squashing instruction PC %s, seq num %i.\n",
                (*squashIt[tid])->threadNumber,
                (*squashIt[tid])->pcState(),
                (*squashIt[tid])->seqNum);

        // Mark the instruction as squashed, and ready to commit so that
        // it can drain out of the pipeline.
        (*squashIt[tid])->setSquashed();

        (*squashIt[tid])->setCanCommit();


        if (squashIt[tid] == instList[tid].begin()) {
            DPRINTF(ROB, "Reached head of instruction list while "
                    "squashing.\n");

            squashIt[tid] = instList[tid].end();

            doneSquashing[tid] = true;

            return;
        }

        InstIt tail_thread = instList[tid].end();
        tail_thread--;

        if ((*squashIt[tid]) == (*tail_thread))
            robTailUpdate = true;

        squashIt[tid]--;
    }


    // Check if ROB is done squashing.
    if ((*squashIt[tid])->seqNum <= squashedSeqNum[tid]) {
        DPRINTF(ROB, "[tid:%i] Done squashing instructions.\n",
                tid);

        squashIt[tid] = instList[tid].end();

        doneSquashing[tid] = true;
    }

    if (robTailUpdate) {
        updateTail();
    }
}


void
ROB::updateHead()
{
    InstSeqNum lowest_num = 0;
    bool first_valid = true;

    // @todo: set ActiveThreads through ROB or CPU
    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (instList[tid].empty())
            continue;

        if (first_valid) {
            head = instList[tid].begin();
            lowest_num = (*head)->seqNum;
            first_valid = false;
            continue;
        }

        InstIt head_thread = instList[tid].begin();

        DynInstPtr head_inst = (*head_thread);

        assert(head_inst != 0);

        if (head_inst->seqNum < lowest_num) {
            head = head_thread;
            lowest_num = head_inst->seqNum;
        }
    }

    if (first_valid) {
        head = instList[0].end();
    }

}

void
ROB::updateTail()
{
    tail = instList[0].end();
    bool first_valid = true;

    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (instList[tid].empty()) {
            continue;
        }

        // If this is the first valid then assign w/out
        // comparison
        if (first_valid) {
            tail = instList[tid].end();
            tail--;
            first_valid = false;
            continue;
        }

        // Assign new tail if this thread's tail is younger
        // than our current "tail high"
        InstIt tail_thread = instList[tid].end();
        tail_thread--;

        if ((*tail_thread)->seqNum > (*tail)->seqNum) {
            tail = tail_thread;
        }
    }
}


void
ROB::squash(InstSeqNum squash_num, ThreadID tid)
{
    if (isEmpty(tid)) {
        DPRINTF(ROB, "Does not need to squash due to being empty "
                "[sn:%llu]\n",
                squash_num);

        return;
    }

    DPRINTF(ROB, "Starting to squash within the ROB.\n");

    robStatus[tid] = ROBSquashing;

    doneSquashing[tid] = false;

    squashedSeqNum[tid] = squash_num;

    if (!instList[tid].empty()) {
        InstIt tail_thread = instList[tid].end();
        tail_thread--;

        squashIt[tid] = tail_thread;

        doSquash(tid);
    }
}

const DynInstPtr&
ROB::readHeadInst(ThreadID tid)
{
    if (threadEntries[tid] != 0) {
        InstIt head_thread = instList[tid].begin();

        assert((*head_thread)->isInROB());

        return *head_thread;
    } else {
        return dummyInst;
    }
}

DynInstPtr
ROB::readTailInst(ThreadID tid)
{
    InstIt tail_thread = instList[tid].end();
    tail_thread--;

    return *tail_thread;
}

ROB::ROBStats::ROBStats(statistics::Group *parent)
  : statistics::Group(parent, "rob"),
    ADD_STAT(reads, statistics::units::Count::get(),
        "The number of ROB reads"),
    ADD_STAT(writes, statistics::units::Count::get(),
        "The number of ROB writes")
{
}

DynInstPtr
ROB::findInst(ThreadID tid, InstSeqNum squash_inst)
{
    for (InstIt it = instList[tid].begin(); it != instList[tid].end(); it++) {
        if ((*it)->seqNum == squash_inst) {
            return *it;
        }
    }
    return NULL;
}

void
ROB::dump(ThreadID tid)
{
    for (InstIt it = instList[tid].begin(); it != instList[tid].end(); it++) {
        DynInstPtr inst = *it;
        cprintf("[sn:%llu] (PC %s) : %s\n",
                inst->seqNum, inst->pcState(),
                inst->staticInst->disassemble(inst->pcState().instAddr()));
    }
}

} // namespace runahead
} // namespace gem5
