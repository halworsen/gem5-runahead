/*
 * Copyright 2014 Google, Inc.
 * Copyright (c) 2010-2014, 2017, 2020 ARM Limited
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
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
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

#include "cpu/runahead/commit.hh"

#include <algorithm>
#include <set>
#include <string>

#include "base/compiler.hh"
#include "base/loader/symtab.hh"
#include "base/logging.hh"
#include "config/the_isa.hh"
#include "cpu/base.hh"
#include "cpu/checker/cpu.hh"
#include "cpu/exetrace.hh"
#include "cpu/runahead/cpu.hh"
#include "cpu/runahead/dyn_inst.hh"
#include "cpu/runahead/limits.hh"
#include "cpu/runahead/thread_state.hh"
#include "cpu/timebuf.hh"
#include "debug/Activity.hh"
#include "debug/Commit.hh"
#include "debug/RunaheadCommit.hh"
#include "debug/CommitRate.hh"
#include "debug/Drain.hh"
#include "debug/ExecFaulting.hh"
#include "debug/HtmCpu.hh"
#include "debug/O3PipeView.hh"
#include "params/BaseRunaheadCPU.hh"
#include "sim/faults.hh"
#include "sim/full_system.hh"

namespace gem5
{

namespace runahead
{

void
Commit::processTrapEvent(ThreadID tid, bool wasRunahead)
{
    // If the trap was scheduled in runahead but we've since exited, don't squash
    if (wasRunahead && !cpu->inRunahead(tid)) {
        return;
    }

    // This will get reset by commit if it was switched out at the
    // time of this event processing.
    trapSquash[tid] = true;
}

Commit::Commit(CPU *_cpu, const BaseRunaheadCPUParams &params)
    : commitPolicy(params.smtCommitPolicy),
      cpu(_cpu),
      runaheadExitDeadline(params.runaheadExitDeadline),
      iewToCommitDelay(params.iewToCommitDelay),
      commitToIEWDelay(params.commitToIEWDelay),
      renameToROBDelay(params.renameToROBDelay),
      fetchToCommitDelay(params.commitToFetchDelay),
      renameWidth(params.renameWidth),
      commitWidth(params.commitWidth),
      numThreads(params.numThreads),
      drainPending(false),
      drainImminent(false),
      trapLatency(params.trapLatency),
      canHandleInterrupts(true),
      avoidQuiesceLiveLock(false),
      stats(_cpu, this)
{
    if (commitWidth > MaxWidth)
        fatal("commitWidth (%d) is larger than compiled limit (%d),\n"
             "\tincrease MaxWidth in src/cpu/runahead/limits.hh\n",
             commitWidth, static_cast<int>(MaxWidth));

    _status = Active;
    _nextStatus = Inactive;

    if (commitPolicy == CommitPolicy::RoundRobin) {
        //Set-Up Priority List
        for (ThreadID tid = 0; tid < numThreads; tid++) {
            priority_list.push_back(tid);
        }
    }

    for (ThreadID tid = 0; tid < MaxThreads; tid++) {
        commitStatus[tid] = Idle;
        changedROBNumEntries[tid] = false;
        trapSquash[tid] = false;
        tcSquash[tid] = false;
        squashAfterInst[tid] = nullptr;
        pc[tid].reset(params.isa[0]->newPCState());
        youngestSeqNum[tid] = 0;
        lastCommitedSeqNum[tid] = 0;
        trapInFlight[tid] = false;
        committedStores[tid] = false;
        checkEmptyROB[tid] = false;
        renameMap[tid] = nullptr;
        htmStarts[tid] = 0;
        htmStops[tid] = 0;
    }
    interrupt = NoFault;

    // Setup runahead exit policy
    if (params.runaheadExitPolicy == "Eager") {
        runaheadExitPolicy = REExitPolicy::Eager;
    } else if (params.runaheadExitPolicy == "MinimumWork") {
        runaheadExitPolicy = REExitPolicy::MinimumWork;
        minRunaheadWork = params.minRunaheadWork;
    } else if (params.runaheadExitPolicy == "DynamicDelayed") {
        runaheadExitPolicy = REExitPolicy::DynamicDelayed;
    } else {
        runaheadExitPolicy = REExitPolicy::Eager;
    }
}

std::string Commit::name() const { return cpu->name() + ".commit"; }

void
Commit::regProbePoints()
{
    ppCommit = new ProbePointArg<DynInstPtr>(
            cpu->getProbeManager(), "Commit");
    ppCommitStall = new ProbePointArg<DynInstPtr>(
            cpu->getProbeManager(), "CommitStall");
    ppSquash = new ProbePointArg<DynInstPtr>(
            cpu->getProbeManager(), "Squash");
}

Commit::CommitStats::CommitStats(CPU *cpu, Commit *commit)
    : statistics::Group(cpu, "commit"),
      ADD_STAT(commitSquashedInsts, statistics::units::Count::get(),
               "The number of squashed insts skipped by commit"),
      ADD_STAT(commitNonSpecStalls, statistics::units::Count::get(),
               "The number of times commit has been forced to stall to "
               "communicate backwards"),
      ADD_STAT(branchMispredicts, statistics::units::Count::get(),
               "The number of times a branch was mispredicted"),
      ADD_STAT(realBranchMispredicts, statistics::units::Count::get(),
               "The number of times a branch was mispredicted in normal mode"),
      ADD_STAT(runaheadBranchMispredicts, statistics::units::Count::get(),
               "The number of times a branch was mispredicted in runahead mode"),
      ADD_STAT(numCommittedDist, statistics::units::Count::get(),
               "Number of insts commited each cycle"),
      ADD_STAT(instsCommitted, statistics::units::Count::get(),
               "Number of instructions committed"),
      ADD_STAT(opsCommitted, statistics::units::Count::get(),
               "Number of ops (including micro ops) committed"),
      ADD_STAT(memRefs, statistics::units::Count::get(),
               "Number of memory references committed"),
      ADD_STAT(loads, statistics::units::Count::get(), "Number of loads committed"),
      ADD_STAT(amos, statistics::units::Count::get(),
               "Number of atomic instructions committed"),
      ADD_STAT(membars, statistics::units::Count::get(),
               "Number of memory barriers committed"),
      ADD_STAT(branches, statistics::units::Count::get(),
               "Number of branches committed"),
      ADD_STAT(vectorInstructions, statistics::units::Count::get(),
               "Number of committed Vector instructions."),
      ADD_STAT(floating, statistics::units::Count::get(),
               "Number of committed floating point instructions."),
      ADD_STAT(integer, statistics::units::Count::get(),
               "Number of committed integer instructions."),
      ADD_STAT(functionCalls, statistics::units::Count::get(),
               "Number of function calls committed."),
      ADD_STAT(committedInstType, statistics::units::Count::get(),
               "Class of committed instruction"),
      ADD_STAT(squashCycles, statistics::units::Cycle::get(),
               "Number of cycles commit is blocked due to the ROB squashing"),
      ADD_STAT(commitEligibleSamples, statistics::units::Cycle::get(),
               "number cycles where commit BW limit reached"),
      ADD_STAT(loadsAtROBHead, statistics::units::Count::get(),
               "Amount of cycles with loads at the head of the ROB during commit"),
      ADD_STAT(lllAtROBHead, statistics::units::Cycle::get(),
               "Total amount of cycles with LLLs at the ROB head"),
      ADD_STAT(instsPseudoretired, statistics::units::Count::get(),
               "Number of instructions committed in runahead"),
      ADD_STAT(commitPoisonedInsts, statistics::units::Count::get(),
               "Number of poisoned instructions retired by commit"),
      ADD_STAT(runaheadOverhead, statistics::units::Cycle::get(),
               "Distribution of cycles spent to exit from runahead"),
      ADD_STAT(totalRunaheadOverhead, statistics::units::Cycle::get(),
               "Total amount of cycles spent exiting runahead"),
      ADD_STAT(runaheadExitCause, statistics::units::Count::get(),
               "Final cause for exiting runahead")
{
    using namespace statistics;

    commitSquashedInsts.prereq(commitSquashedInsts);
    commitNonSpecStalls.prereq(commitNonSpecStalls);
    branchMispredicts.prereq(branchMispredicts);
    realBranchMispredicts.prereq(realBranchMispredicts);
    runaheadBranchMispredicts.prereq(runaheadBranchMispredicts);

    numCommittedDist
        .init(0,commit->commitWidth,1)
        .flags(statistics::pdf);

    instsCommitted
        .init(cpu->numThreads)
        .flags(total);

    opsCommitted
        .init(cpu->numThreads)
        .flags(total);

    memRefs
        .init(cpu->numThreads)
        .flags(total);

    loads
        .init(cpu->numThreads)
        .flags(total);

    amos
        .init(cpu->numThreads)
        .flags(total);

    membars
        .init(cpu->numThreads)
        .flags(total);

    branches
        .init(cpu->numThreads)
        .flags(total);

    vectorInstructions
        .init(cpu->numThreads)
        .flags(total);

    floating
        .init(cpu->numThreads)
        .flags(total);

    integer
        .init(cpu->numThreads)
        .flags(total);

    functionCalls
        .init(commit->numThreads)
        .flags(total);

    committedInstType
        .init(commit->numThreads,enums::Num_OpClass)
        .flags(total | pdf | dist);

    committedInstType.ysubnames(enums::OpClassStrings);

    squashCycles.prereq(squashCycles);

    loadsAtROBHead.prereq(loadsAtROBHead);
    lllAtROBHead.prereq(lllAtROBHead);
    instsPseudoretired
        .init(cpu->numThreads)
        .flags(total);

    runaheadOverhead
        .init(10)
        .flags(statistics::total);
    totalRunaheadOverhead.prereq(totalRunaheadOverhead);

    runaheadExitCause
        .init(REExitCause::Deadline + 1)
        .flags(statistics::total);
}

void
Commit::setThreads(std::vector<ThreadState *> &threads)
{
    thread = threads;
}

void
Commit::setTimeBuffer(TimeBuffer<TimeStruct> *tb_ptr)
{
    timeBuffer = tb_ptr;

    // Setup wire to send information back to IEW.
    toIEW = timeBuffer->getWire(0);

    // Setup wire to read data from IEW (for the ROB).
    robInfoFromIEW = timeBuffer->getWire(-iewToCommitDelay);
}

void
Commit::setFetchQueue(TimeBuffer<FetchStruct> *fq_ptr)
{
    fetchQueue = fq_ptr;

    // Setup wire to get instructions from rename (for the ROB).
    fromFetch = fetchQueue->getWire(-fetchToCommitDelay);
}

void
Commit::setRenameQueue(TimeBuffer<RenameStruct> *rq_ptr)
{
    renameQueue = rq_ptr;

    // Setup wire to get instructions from rename (for the ROB).
    fromRename = renameQueue->getWire(-renameToROBDelay);
}

void
Commit::setIEWQueue(TimeBuffer<IEWStruct> *iq_ptr)
{
    iewQueue = iq_ptr;

    // Setup wire to get instructions from IEW.
    fromIEW = iewQueue->getWire(-iewToCommitDelay);
}

void
Commit::setIEWStage(IEW *iew_stage)
{
    iewStage = iew_stage;
}

void
Commit::setActiveThreads(std::list<ThreadID> *at_ptr)
{
    activeThreads = at_ptr;
}

void
Commit::setRenameMap(UnifiedRenameMap rm_ptr[])
{
    for (ThreadID tid = 0; tid < numThreads; tid++)
        renameMap[tid] = &rm_ptr[tid];
}

void Commit::setROB(ROB *rob_ptr) { rob = rob_ptr; }

void
Commit::startupStage()
{
    rob->setActiveThreads(activeThreads);
    rob->resetEntries();

    // Broadcast the number of free entries.
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        toIEW->commitInfo[tid].usedROB = true;
        toIEW->commitInfo[tid].freeROBEntries = rob->numFreeEntries(tid);
        toIEW->commitInfo[tid].emptyROB = true;
    }

    // Commit must broadcast the number of free entries it has at the
    // start of the simulation, so it starts as active.
    cpu->activateStage(CPU::CommitIdx);

    cpu->activityThisCycle();
}

void
Commit::clearStates(ThreadID tid)
{
    commitStatus[tid] = Idle;
    changedROBNumEntries[tid] = false;
    checkEmptyROB[tid] = false;
    trapInFlight[tid] = false;
    committedStores[tid] = false;
    trapSquash[tid] = false;
    tcSquash[tid] = false;
    pc[tid].reset(cpu->tcBase(tid)->getIsaPtr()->newPCState());
    lastCommitedSeqNum[tid] = 0;
    squashAfterInst[tid] = NULL;
}

void Commit::drain() { drainPending = true; }

void
Commit::drainResume()
{
    drainPending = false;
    drainImminent = false;
}

void
Commit::drainSanityCheck() const
{
    assert(isDrained());
    rob->drainSanityCheck();

    // hardware transactional memory
    // cannot drain partially through a transaction
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (executingHtmTransaction(tid)) {
            panic("cannot drain partially through a HTM transaction");
        }
    }
}

bool
Commit::isDrained() const
{
    /* Make sure no one is executing microcode. There are two reasons
     * for this:
     * - Hardware virtualized CPUs can't switch into the middle of a
     *   microcode sequence.
     * - The current fetch implementation will most likely get very
     *   confused if it tries to start fetching an instruction that
     *   is executing in the middle of a ucode sequence that changes
     *   address mappings. This can happen on for example x86.
     */
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (pc[tid]->microPC() != 0)
            return false;
    }

    /* Make sure that all instructions have finished committing before
     * declaring the system as drained. We want the pipeline to be
     * completely empty when we declare the CPU to be drained. This
     * makes debugging easier since CPU handover and restoring from a
     * checkpoint with a different CPU should have the same timing.
     */
    return rob->isEmpty() &&
        interrupt == NoFault;
}

void
Commit::takeOverFrom()
{
    _status = Active;
    _nextStatus = Inactive;
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        commitStatus[tid] = Idle;
        changedROBNumEntries[tid] = false;
        trapSquash[tid] = false;
        tcSquash[tid] = false;
        squashAfterInst[tid] = NULL;
    }
    rob->takeOverFrom();
}

void
Commit::deactivateThread(ThreadID tid)
{
    std::list<ThreadID>::iterator thread_it = std::find(priority_list.begin(),
            priority_list.end(), tid);

    if (thread_it != priority_list.end()) {
        priority_list.erase(thread_it);
    }
}

bool
Commit::executingHtmTransaction(ThreadID tid) const
{
    if (tid == InvalidThreadID)
        return false;
    else
        return (htmStarts[tid] > htmStops[tid]);
}

void
Commit::resetHtmStartsStops(ThreadID tid)
{
    if (tid != InvalidThreadID)
    {
        htmStarts[tid] = 0;
        htmStops[tid] = 0;
    }
}


void
Commit::updateStatus()
{
    // reset ROB changed variable
    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        changedROBNumEntries[tid] = false;

        // Also check if any of the threads has a trap pending
        if (commitStatus[tid] == TrapPending ||
            commitStatus[tid] == FetchTrapPending) {
            _nextStatus = Active;
        }
    }

    if (_nextStatus == Inactive && _status == Active) {
        DPRINTF(Activity, "Deactivating stage.\n");
        cpu->deactivateStage(CPU::CommitIdx);
    } else if (_nextStatus == Active && _status == Inactive) {
        DPRINTF(Activity, "Activating stage.\n");
        cpu->activateStage(CPU::CommitIdx);
    }

    _status = _nextStatus;
}

bool
Commit::changedROBEntries()
{
    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (changedROBNumEntries[tid]) {
            return true;
        }
    }

    return false;
}

size_t
Commit::numROBFreeEntries(ThreadID tid)
{
    return rob->numFreeEntries(tid);
}

void
Commit::generateTrapEvent(ThreadID tid, Fault inst_fault)
{
    DPRINTF(Commit, "Generating trap event for [tid:%i]\n", tid);

    bool inRunahead = cpu->inRunahead(tid);
    EventFunctionWrapper *trap = new EventFunctionWrapper(
        [this, inRunahead, tid]{ processTrapEvent(tid, inRunahead); },
        "Trap", true, Event::CPU_Tick_Pri);

    Cycles latency = std::dynamic_pointer_cast<SyscallRetryFault>(inst_fault) ?
                     cpu->syscallRetryLatency : trapLatency;

    // hardware transactional memory
    if (inst_fault != nullptr &&
        std::dynamic_pointer_cast<GenericHtmFailureFault>(inst_fault)) {
        // TODO
        // latency = default abort/restore latency
        // could also do some kind of exponential back off if desired
    }

    cpu->schedule(trap, cpu->clockEdge(latency));
    trapInFlight[tid] = true;
    thread[tid]->trapPending = true;
}

void
Commit::generateTCEvent(ThreadID tid)
{
    assert(!trapInFlight[tid]);
    DPRINTF(Commit, "Generating TC squash event for [tid:%i]\n", tid);

    tcSquash[tid] = true;
}

void
Commit::signalExitRunahead(ThreadID tid, const DynInstPtr &inst)
{
    DPRINTF(RunaheadCommit, "[tid:%i] Runahead exit signal received, cause inst sn: %llu, PC: %s.\n",
                     tid, inst->seqNum, inst->pcState());

    runaheadExitable[tid] = true;
    runaheadCause[tid] = inst;

    // Handle the signal according to the exit policy
    if (runaheadExitPolicy == REExitPolicy::Eager) {
        DPRINTF(RunaheadCommit, "[tid:%i] Exiting runahead ASAP due to eager exit policy.\n",
                tid);
        exitRunahead[tid] = true;
        stats.runaheadExitCause[stats.REExitCause::EagerExit]++;
    } else if (runaheadExitPolicy == REExitPolicy::MinimumWork && instsPseudoretired[tid] >= minRunaheadWork) {
        DPRINTF(RunaheadCommit, "[tid:%i] Exiting runahead now because minimum work has been done.\n",
            tid, minRunaheadWork);
        exitRunahead[tid] = true;
        stats.runaheadExitCause[stats.REExitCause::MinWorkDone]++;
    } else if (runaheadExitPolicy == DynamicDelayed) {
        panic("dynamic delayed runahead exit is unimplemented!");
    }

    // If we aren't exiting immediately, schedule a deadline event
    InstSeqNum causeSeqNum = inst->seqNum;
    if (!exitRunahead[tid]) {
        EventFunctionWrapper *exitEvent = new EventFunctionWrapper(
            [this, tid, causeSeqNum]{
                DPRINTF(RunaheadCommit, "[tid:%i] Runahead deadline reached for sn:%llu, "
                                        "checking if runahead should exit.\n",
                                        tid, causeSeqNum);

                // Already exited/exiting
                if (!cpu->inRunahead(tid) || exitRunahead[tid])
                    return;

                // We're in a different runahead period
                if (runaheadCause[tid]->seqNum != causeSeqNum)
                    return;

                DPRINTF(RunaheadCommit, "[tid:%i] Runahead was not exited, exiting now runahead due to deadline.");
                exitRunahead[tid] = true;
                stats.runaheadExitCause[stats.REExitCause::Deadline]++;
            },
            "RunaheadExitDeadline", true, Event::CPU_Tick_Pri
        );
        cpu->schedule(exitEvent, curTick() + cpu->clockEdge(runaheadExitDeadline));
    }
}

void
Commit::squashAll(ThreadID tid)
{
    // If we want to include the squashing instruction in the squash,
    // then use one older sequence number.
    // Hopefully this doesn't mess things up.  Basically I want to squash
    // all instructions of this thread.
    InstSeqNum squashed_inst = rob->isEmpty(tid) ?
        lastCommitedSeqNum[tid] : rob->readHeadInst(tid)->seqNum - 1;

    // All younger instructions will be squashed. Set the sequence
    // number as the youngest instruction in the ROB (0 in this case.
    // Hopefully nothing breaks.)
    youngestSeqNum[tid] = lastCommitedSeqNum[tid];

    rob->squash(squashed_inst, tid);
    changedROBNumEntries[tid] = true;

    // Send back the sequence number of the squashed instruction.
    toIEW->commitInfo[tid].doneSeqNum = squashed_inst;
    toIEW->commitInfo[tid].squashTail = rob->isEmpty(tid) ? squashed_inst : rob->readTailInst(tid)->seqNum;

    // Send back the squash signal to tell stages that they should
    // squash.
    toIEW->commitInfo[tid].squash = true;

    // Send back the rob squashing signal so other stages know that
    // the ROB is in the process of squashing.
    toIEW->commitInfo[tid].robSquashing = true;

    toIEW->commitInfo[tid].mispredictInst = NULL;
    toIEW->commitInfo[tid].squashInst = NULL;

    set(toIEW->commitInfo[tid].pc, pc[tid]);
}

void
Commit::squashFromTrap(ThreadID tid)
{
    squashAll(tid);

    DPRINTF(Commit, "Squashing from trap, restarting at PC %s\n", *pc[tid]);

    thread[tid]->trapPending = false;
    thread[tid]->noSquashFromTC = false;
    trapInFlight[tid] = false;

    trapSquash[tid] = false;

    commitStatus[tid] = ROBSquashing;
    cpu->activityThisCycle();
}

void
Commit::squashFromTC(ThreadID tid)
{
    squashAll(tid);

    DPRINTF(Commit, "Squashing from TC, restarting at PC %s\n", *pc[tid]);

    thread[tid]->noSquashFromTC = false;
    assert(!thread[tid]->trapPending);

    commitStatus[tid] = ROBSquashing;
    cpu->activityThisCycle();

    tcSquash[tid] = false;
}

void
Commit::squashFromSquashAfter(ThreadID tid)
{
    DPRINTF(Commit, "Squashing after squash after request, "
            "restarting at PC %s\n", *pc[tid]);

    squashAll(tid);
    // Make sure to inform the fetch stage of which instruction caused
    // the squash. It'll try to re-fetch an instruction executing in
    // microcode unless this is set.
    toIEW->commitInfo[tid].squashInst = squashAfterInst[tid];
    squashAfterInst[tid] = NULL;

    commitStatus[tid] = ROBSquashing;
    cpu->activityThisCycle();
}

void
Commit::squashAfter(ThreadID tid, const DynInstPtr &head_inst)
{
    DPRINTF(Commit, "Executing squash after for [tid:%i] inst [sn:%llu]\n",
            tid, head_inst->seqNum);

    assert(!squashAfterInst[tid] || squashAfterInst[tid] == head_inst);
    commitStatus[tid] = SquashAfterPending;
    squashAfterInst[tid] = head_inst;
}

void
Commit::squashFromRunaheadExit(ThreadID tid)
{
    exitRunahead[tid] = false;
    // start counting cycles to the next committed inst for stats
    runaheadExitCycles = 0;

    // Signal to all stages that they should squash and restore architectural state
    toIEW->commitInfo[tid].squash = true;
    // We will read this signal next cycle to perform an arch restore.
    timeBuffer->getWire(0)->archRestore[tid] = true;

    // Squash up to and including the LLL that caused entry into runahead
    const DynInstPtr &lll = runaheadCause[tid];
    InstSeqNum squashedSeqNum = lll->seqNum - 1;

    DPRINTF(RunaheadCommit, "[tid:%i] [sn:%llu] Performing runahead exit squash\n",
            tid, lll->seqNum);

    youngestSeqNum[tid] = squashedSeqNum;
    toIEW->commitInfo[tid].doneSeqNum = squashedSeqNum;
    toIEW->commitInfo[tid].squashTail = rob->isEmpty(tid) ? squashedSeqNum : rob->readTailInst(tid)->seqNum;

    // Start squashing in the ROB
    commitStatus[tid] = ROBSquashing;
    rob->squash(squashedSeqNum, tid);
    changedROBNumEntries[tid] = true;
    toIEW->commitInfo[tid].robSquashing = true;

    toIEW->commitInfo[tid].mispredictInst = NULL;
    toIEW->commitInfo[tid].squashInst = rob->findInst(tid, squashedSeqNum);

    set(pc[tid], *storedPC[tid]);
    set(toIEW->commitInfo[tid].pc, pc[tid]);

    // Reset any in-flight traps
    trapInFlight[tid] = false;
    thread[tid]->trapPending = false;

    cpu->activityThisCycle();
    // Let the CPU exit runahead mode now that the squash has been signalled
    cpu->exitRunahead(tid);
    runaheadExitable[tid] = false;
}

void
Commit::tick()
{
    wroteToTimeBuffer = false;
    _nextStatus = Inactive;

    if (activeThreads->empty())
        return;

    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    // Count cycles since runahead was exited if swapping to normal from runahead mode
    if (runaheadExitCycles >= 0)
        runaheadExitCycles++;

    // Check if any of the threads are done squashing.  Change the
    // status if they are done.
    while (threads != end) {
        ThreadID tid = *threads++;

        // Clear the bit saying if the thread has committed stores
        // this cycle.
        committedStores[tid] = false;

        if (commitStatus[tid] == ROBSquashing) {

            if (rob->isDoneSquashing(tid)) {
                DPRINTF(Commit, "[tid:%i] ROB done squashing, switching to running.\n", tid);
                commitStatus[tid] = Running;
            } else {
                DPRINTF(Commit,"[tid:%i] Still Squashing, cannot commit any"
                        " insts this cycle.\n", tid);
                rob->doSquash(tid);
                toIEW->commitInfo[tid].robSquashing = true;
                wroteToTimeBuffer = true;
                stats.squashCycles++;
            }
        }
    }

    commit();

    markCompletedInsts();

    threads = activeThreads->begin();

    while (threads != end) {
        ThreadID tid = *threads++;
        DPRINTF(Commit, "[tid:%i] ROB has %d insts & %d free entries.\n",
                tid, rob->countInsts(tid), rob->numFreeEntries(tid));

        if (rob->isEmpty(tid)) {
            continue;
        }

        const DynInstPtr &headInst = rob->readHeadInst(tid);
        if (rob->isHeadReady(tid)) {
            // The ROB has more instructions it can commit. Its next status
            // will be active.
            _nextStatus = Active;

            DPRINTF(Commit,"[tid:%i] Instruction [sn:%llu] PC %s is head of"
                    " ROB and ready to commit\n",
                    tid, headInst->seqNum, headInst->pcState());
        } else {
            ppCommitStall->notify(headInst);

            DPRINTF(Commit,"[tid:%i] Can't commit, Instruction [sn:%llu] PC "
                    "%s is head of ROB and not ready\n",
                    tid, headInst->seqNum, headInst->pcState());
        }
    }

    threads = activeThreads->begin();
    while (threads != end) {
        ThreadID tid = *threads++;

        wasRunahead[tid] = cpu->inRunahead(tid);

        // If we signalled to ourself that we should perform an arch restore, do so now
        if (timeBuffer->getWire(-1)->archRestore[tid])
            cpu->restoreCheckpointState(tid);
    }

    if (wroteToTimeBuffer) {
        DPRINTF(Activity, "Activity This Cycle.\n");
        cpu->activityThisCycle();
    }

    updateStatus();
}

void
Commit::handleInterrupt()
{
    // Verify that we still have an interrupt to handle
    if (!cpu->checkInterrupts(0)) {
        DPRINTF(Commit, "Pending interrupt is cleared by requestor before "
                "it got handled. Restart fetching from the orig path.\n");
        toIEW->commitInfo[0].clearInterrupt = true;
        interrupt = NoFault;
        avoidQuiesceLiveLock = true;
        return;
    }

    // Wait until all in flight instructions are finished before enterring
    // the interrupt.
    if (canHandleInterrupts && cpu->instList.empty()) {
        // Squash or record that I need to squash this cycle if
        // an interrupt needed to be handled.
        DPRINTF(Commit, "Interrupt detected.\n");

        // Clear the interrupt now that it's going to be handled
        toIEW->commitInfo[0].clearInterrupt = true;

        assert(!thread[0]->noSquashFromTC);
        thread[0]->noSquashFromTC = true;

        if (cpu->checker) {
            cpu->checker->handlePendingInt();
        }

        // CPU will handle interrupt. Note that we ignore the local copy of
        // interrupt. This is because the local copy may no longer be the
        // interrupt that the interrupt controller thinks is being handled.
        cpu->processInterrupts(cpu->getInterrupts());

        thread[0]->noSquashFromTC = false;

        commitStatus[0] = TrapPending;

        interrupt = NoFault;

        // Generate trap squash event.
        generateTrapEvent(0, interrupt);

        avoidQuiesceLiveLock = false;
    } else {
        DPRINTF(Commit, "Interrupt pending: instruction is %sin "
                "flight, ROB is %sempty\n",
                canHandleInterrupts ? "not " : "",
                cpu->instList.empty() ? "" : "not " );
    }
}

void
Commit::propagateInterrupt()
{
    // Don't propagate intterupts if we are currently handling a trap or
    // in draining and the last observable instruction has been committed.
    // Also don't propagate while in runahead or waiting for arch restores
    if (commitStatus[0] == TrapPending || interrupt || trapSquash[0] ||
        tcSquash[0] || drainImminent || cpu->inRunahead(0) ||
        timeBuffer->getWire(-1)->archRestore[0])
        return;

    // Process interrupts if interrupts are enabled, not in PAL
    // mode, and no other traps or external squashes are currently
    // pending.
    // @todo: Allow other threads to handle interrupts.

    // Get any interrupt that happened
    interrupt = cpu->getInterrupts();

    // Tell fetch that there is an interrupt pending.  This
    // will make fetch wait until it sees a non PAL-mode PC,
    // at which point it stops fetching instructions.
    if (interrupt != NoFault)
        toIEW->commitInfo[0].interruptPending = true;
}

void
Commit::commit()
{
    if (FullSystem) {
        // Check if we have a interrupt and get read to handle it
        if (cpu->checkInterrupts(0))
            propagateInterrupt();
    }

    ////////////////////////////////////
    // Check for any possible squashes, handle them first
    ////////////////////////////////////
    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    int num_squashing_threads = 0;

    while (threads != end) {
        ThreadID tid = *threads++;

        // Not sure which one takes priority.  I think if we have
        // both, that's a bad sign.
        if (trapSquash[tid]) {
            assert(!tcSquash[tid]);
            squashFromTrap(tid);

            // If the thread is trying to exit (i.e., an exit syscall was
            // executed), this trapSquash was originated by the exit
            // syscall earlier. In this case, schedule an exit event in
            // the next cycle to fully terminate this thread
            if (cpu->isThreadExiting(tid))
                cpu->scheduleThreadExitEvent(tid);
        } else if (tcSquash[tid]) {
            assert(commitStatus[tid] != TrapPending);
            squashFromTC(tid);
        } else if (commitStatus[tid] == SquashAfterPending) {
            // Make sure we're not about to do a squash after initiated by a stale runahead inst
            if (!(wasRunahead[tid] && !cpu->inRunahead(tid))) {
                // A squash from the previous cycle of the commit stage (i.e.,
                // commitInsts() called squashAfter) is pending. Squash the
                // thread now.
                squashFromSquashAfter(tid);
            } else {
                commitStatus[tid] = Running;
            }
        } else if (exitRunahead[tid]) {
            squashFromRunaheadExit(tid);
        }

        // Squashed sequence number must be older than youngest valid
        // instruction in the ROB. This prevents squashes from younger
        // instructions overriding squashes from older instructions.
        if (fromIEW->squash[tid] &&
            commitStatus[tid] != TrapPending &&
            fromIEW->squashedSeqNum[tid] <= youngestSeqNum[tid]) {

            if (fromIEW->mispredictInst[tid]) {
                DPRINTF(Commit,
                    "[tid:%i] Squashing due to branch mispred "
                    "PC:%#x [sn:%llu]\n",
                    tid,
                    fromIEW->mispredictInst[tid]->pcState().instAddr(),
                    fromIEW->squashedSeqNum[tid]);
            } else if (fromIEW->runaheadInst[tid]) {
                DPRINTF(Commit,
                    "[tid:%i] Squashing due to runahead exit "
                    "PC:%#x [sn:%llu]\n",
                    tid,
                    fromIEW->runaheadInst[tid]->pcState().instAddr(),
                    fromIEW->squashedSeqNum[tid]);
            } else {
                DPRINTF(Commit,
                    "[tid:%i] Squashing due to order violation [sn:%llu]\n",
                    tid, fromIEW->squashedSeqNum[tid]);
            }

            DPRINTF(Commit, "[tid:%i] Redirecting to PC %#x\n",
                    tid, *fromIEW->pc[tid]);

            commitStatus[tid] = ROBSquashing;

            // If we want to include the squashing instruction in the squash,
            // then use one older sequence number.
            InstSeqNum squashed_inst = fromIEW->squashedSeqNum[tid];

            if (fromIEW->includeSquashInst[tid]) {
                squashed_inst--;
            }

            // All younger instructions will be squashed. Set the sequence
            // number as the youngest instruction in the ROB.
            youngestSeqNum[tid] = squashed_inst;

            rob->squash(squashed_inst, tid);
            changedROBNumEntries[tid] = true;

            toIEW->commitInfo[tid].doneSeqNum = squashed_inst;
            toIEW->commitInfo[tid].squashTail = rob->isEmpty(tid) ? squashed_inst : rob->readTailInst(tid)->seqNum;

            toIEW->commitInfo[tid].squash = true;

            // Send back the rob squashing signal so other stages know that
            // the ROB is in the process of squashing.
            toIEW->commitInfo[tid].robSquashing = true;

            toIEW->commitInfo[tid].mispredictInst =
                fromIEW->mispredictInst[tid];
            toIEW->commitInfo[tid].branchTaken =
                fromIEW->branchTaken[tid];
            toIEW->commitInfo[tid].squashInst =
                                    rob->findInst(tid, squashed_inst);
            if (toIEW->commitInfo[tid].mispredictInst) {
                if (toIEW->commitInfo[tid].mispredictInst->isUncondCtrl()) {
                     toIEW->commitInfo[tid].branchTaken = true;
                }
                ++stats.branchMispredicts;
                if (fromIEW->mispredictInst[tid]->isRunahead())
                    ++stats.runaheadBranchMispredicts;
                else
                    ++stats.realBranchMispredicts;
            }

            set(toIEW->commitInfo[tid].pc, fromIEW->pc[tid]);
        }

        if (commitStatus[tid] == ROBSquashing) {
            num_squashing_threads++;
        }
    }

    // If commit is currently squashing, then it will have activity for the
    // next cycle. Set its next status as active.
    if (num_squashing_threads) {
        _nextStatus = Active;
    }

    if (num_squashing_threads != numThreads) {
        // If we're not currently squashing, then get instructions.
        getInsts();

        // Try to commit any instructions.
        commitInsts();
    }

    //Check for any activity
    threads = activeThreads->begin();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (changedROBNumEntries[tid]) {
            toIEW->commitInfo[tid].usedROB = true;
            toIEW->commitInfo[tid].freeROBEntries = rob->numFreeEntries(tid);

            wroteToTimeBuffer = true;
            changedROBNumEntries[tid] = false;
            if (rob->isEmpty(tid))
                checkEmptyROB[tid] = true;
        }

        // ROB is only considered "empty" for previous stages if: a)
        // ROB is empty, b) there are no outstanding stores, c) IEW
        // stage has received any information regarding stores that
        // committed.
        // c) is checked by making sure to not consider the ROB empty
        // on the same cycle as when stores have been committed.
        // @todo: Make this handle multi-cycle communication between
        // commit and IEW.
        if (checkEmptyROB[tid] && rob->isEmpty(tid) &&
            !iewStage->hasStoresToWB(tid) && !committedStores[tid]) {
            checkEmptyROB[tid] = false;
            toIEW->commitInfo[tid].usedROB = true;
            toIEW->commitInfo[tid].emptyROB = true;
            toIEW->commitInfo[tid].freeROBEntries = rob->numFreeEntries(tid);
            wroteToTimeBuffer = true;
        }

    }
}

void
Commit::commitInsts()
{
    ////////////////////////////////////
    // Handle commit
    // Note that commit will be handled prior to putting new
    // instructions in the ROB so that the ROB only tries to commit
    // instructions it has in this current cycle, and not instructions
    // it is writing in during this cycle.  Can't commit and squash
    // things at the same time...
    ////////////////////////////////////

    DPRINTF(Commit, "Trying to commit instructions in the ROB.\n");

    unsigned num_committed = 0;

    DynInstPtr head_inst;

    // Commit as many instructions as possible until the commit bandwidth
    // limit is reached, or it becomes impossible to commit any more.
    while (num_committed < commitWidth) {
        // hardware transactionally memory
        // If executing within a transaction,
        // need to handle interrupts specially

        ThreadID commit_thread = getCommittingThread();

        // Check for any interrupt that we've already squashed for
        // and start processing it.
        if (interrupt != NoFault) {
            // If inside a transaction, postpone interrupts
            if (executingHtmTransaction(commit_thread)) {
                cpu->clearInterrupts(0);
                toIEW->commitInfo[0].clearInterrupt = true;
                interrupt = NoFault;
                avoidQuiesceLiveLock = true;
            } else {
                handleInterrupt();
            }
        }

        // ThreadID commit_thread = getCommittingThread();

        if (commit_thread == -1)
            break;

        head_inst = rob->readHeadInst(commit_thread);
        if (!head_inst)
            break;

        ThreadID tid = head_inst->threadNumber;
        assert(tid == commit_thread);

        // If the ROB head isn't ready, investigate if it's a load we should run ahead of
        if (!rob->isHeadReady(commit_thread)) {
            // Must be a load with an in-flight memory request to cause runahead
            if (!head_inst->isLoad() || !head_inst->hasRequest()) {
                break;
            }

            ++stats.loadsAtROBHead;

            gem5::runahead::LSQ::LSQRequest *lsqRequest = head_inst->savedRequest;
            // That request must not be completed
            // This may be unnecessary? Load may be marked as ready when the request completes
            if (lsqRequest == nullptr || lsqRequest->isComplete()) {
                break;
            }

            DPRINTF(RunaheadCommit,
                    "[tid:%i] In-flight load reached the head of the ROB during commit "
                    "[sn:%llu] (PC %s). Associated requests:\n",
                    tid, head_inst->seqNum, head_inst->pcState());

            // Can't use the stored depth on the inst because it is only updated when pkts respond
            for (int idx = 0; idx < lsqRequest->_reqs.size(); idx++) {
                RequestPtr request = lsqRequest->req(idx);
                int depth = request->getAccessDepth();

                DPRINTF(RunaheadCommit,
                    "[tid:%i] Request #%d hit at depth %d\n",
                    tid, idx+1, depth);

                if (depth >= cpu->lllDepthThreshold) {
                    ++stats.lllAtROBHead;

                    // If not already in runahead, try to enter it
                    // If in runahead, make sure the load isn't already poisoned (waiting to drain)
                    if (!cpu->inRunahead(tid)) {
                        cpu->enterRunahead(tid);
                    } else if (!head_inst->isPoisoned()) {
                        // If in runahead, immediately "complete" it to avoid blocking on it
                        assert(head_inst->isRunahead());
                        DPRINTF(RunaheadCommit,
                                "[tid:%i] Load was a runahead LLL. Attempting to forge response.\n", tid);
                        // Tell the CPU to deal with it. This is kinda ugly, LSQ should handle these
                        cpu->handleRunaheadLLL(head_inst);
                    }

                    break;
                }
            }

            break;
        }

        DPRINTF(Commit,
                "Trying to commit head instruction, [tid:%i] [sn:%llu]\n",
                tid, head_inst->seqNum);

        // If the head instruction is squashed, it is ready to retire
        // (be removed from the ROB) at any time.
        if (head_inst->isSquashed()) {

            DPRINTF(Commit, "Retiring squashed instruction from "
                    "ROB.\n");

            rob->retireHead(commit_thread);

            ++stats.commitSquashedInsts;
            // Notify potential listeners that this instruction is squashed
            ppSquash->notify(head_inst);

            // Record that the number of ROB entries has changed.
            changedROBNumEntries[tid] = true;
        } else {
            set(pc[tid], head_inst->pcState());

            // Try to commit the head instruction.
            bool commit_success = commitHead(head_inst, num_committed);

            if (commit_success) {
                ++num_committed;
                stats.committedInstType[tid][head_inst->opClass()]++;
                if (runaheadExitCycles != -1) {
                    stats.runaheadOverhead.sample(runaheadExitCycles);
                    stats.totalRunaheadOverhead += runaheadExitCycles;
                    runaheadExitCycles = -1;
                }
                ppCommit->notify(head_inst);

                // hardware transactional memory

                // update nesting depth
                if (head_inst->isHtmStart())
                    htmStarts[tid]++;

                // sanity check
                if (head_inst->inHtmTransactionalState()) {
                    assert(executingHtmTransaction(tid));
                } else {
                    assert(!executingHtmTransaction(tid));
                }

                // update nesting depth
                if (head_inst->isHtmStop())
                    htmStops[tid]++;

                changedROBNumEntries[tid] = true;

                // Set the doneSeqNum to the youngest committed instruction.
                toIEW->commitInfo[tid].doneSeqNum = head_inst->seqNum;
                toIEW->commitInfo[tid].squashTail = rob->isEmpty(tid) ? head_inst->seqNum : rob->readTailInst(tid)->seqNum;

                if (tid == 0)
                    canHandleInterrupts = !head_inst->isDelayedCommit();

                // at this point store conditionals should either have
                // been completed or predicated false
                assert(!head_inst->isStoreConditional() ||
                       head_inst->isCompleted() ||
                       !head_inst->readPredicate());

                // Updates misc. registers.
                head_inst->updateMiscRegs();

                // Incremental update of architectural state checkpoint
                // if (!head_inst->isRunahead()) {
                //     cpu->updateArchCheckpoint(tid, head_inst);
                // }

                // Check instruction execution if it successfully commits and
                // is not carrying a fault.
                if (cpu->checker) {
                    cpu->checker->verify(head_inst);
                }

                cpu->traceFunctions(pc[tid]->instAddr());

                head_inst->staticInst->advancePC(*pc[tid]);

                // Keep track of the last sequence number commited
                lastCommitedSeqNum[tid] = head_inst->seqNum;

                // If this is an instruction that doesn't play nicely with
                // others squash everything and restart fetch
                if (head_inst->isSquashAfter())
                    squashAfter(tid, head_inst);

                if (drainPending) {
                    if (pc[tid]->microPC() == 0 && interrupt == NoFault &&
                        !thread[tid]->trapPending) {
                        // Last architectually committed instruction.
                        // Squash the pipeline, stall fetch, and use
                        // drainImminent to disable interrupts
                        DPRINTF(Drain, "Draining: %i:%s\n", tid, *pc[tid]);
                        squashAfter(tid, head_inst);
                        cpu->commitDrained(tid);
                        drainImminent = true;
                    }
                }

                bool onInstBoundary = !head_inst->isMicroop() ||
                                      head_inst->isLastMicroop() ||
                                      !head_inst->isDelayedCommit();

                if (onInstBoundary) {
                    int count = 0;
                    Addr oldpc;
                    // Make sure we're not currently updating state while
                    // handling PC events.
                    assert(!thread[tid]->noSquashFromTC &&
                           !thread[tid]->trapPending);
                    do {
                        oldpc = pc[tid]->instAddr();
                        thread[tid]->pcEventQueue.service(
                                oldpc, thread[tid]->getTC());
                        count++;
                    } while (oldpc != pc[tid]->instAddr());
                    if (count > 1) {
                        DPRINTF(Commit,
                                "PC skip function event, stopping commit\n");
                        break;
                    }
                }

                // Check if an instruction just enabled interrupts and we've
                // previously had an interrupt pending that was not handled
                // because interrupts were subsequently disabled before the
                // pipeline reached a place to handle the interrupt. In that
                // case squash now to make sure the interrupt is handled.
                //
                // If we don't do this, we might end up in a live lock
                // situation.
                if (!interrupt && avoidQuiesceLiveLock &&
                    onInstBoundary && cpu->checkInterrupts(0))
                    squashAfter(tid, head_inst);
            } else {
                DPRINTF(Commit, "Unable to commit head instruction PC:%s "
                        "[tid:%i] [sn:%llu].\n",
                        head_inst->pcState(), tid ,head_inst->seqNum);
                break;
            }
        }
    }

    DPRINTF(CommitRate, "%i\n", num_committed);
    stats.numCommittedDist.sample(num_committed);

    if (num_committed == commitWidth) {
        stats.commitEligibleSamples++;
    }
}

bool
Commit::commitHead(const DynInstPtr &head_inst, unsigned inst_num)
{
    assert(head_inst);

    ThreadID tid = head_inst->threadNumber;

    // If the instruction is not executed yet, then it will need extra
    // handling.  Signal backwards that it should be executed.
    if (!head_inst->isExecuted()) {
        // Make sure we are only trying to commit un-executed instructions we
        // think are possible.
        assert(head_inst->isNonSpeculative() || head_inst->isStoreConditional()
               || head_inst->isReadBarrier() || head_inst->isWriteBarrier()
               || head_inst->isAtomic()
               || (head_inst->isLoad() && head_inst->strictlyOrdered()));

        DPRINTF(Commit,
                "Encountered a barrier or non-speculative "
                "instruction [tid:%i] [sn:%llu] "
                "at the head of the ROB, PC %s.\n",
                tid, head_inst->seqNum, head_inst->pcState());

        if (inst_num > 0 || iewStage->hasStoresToWB(tid)) {
            DPRINTF(Commit,
                    "[tid:%i] [sn:%llu] "
                    "Waiting for all stores to writeback.\n",
                    tid, head_inst->seqNum);
            return false;
        }

        toIEW->commitInfo[tid].nonSpecSeqNum = head_inst->seqNum;

        // Change the instruction so it won't try to commit again until
        // it is executed.
        head_inst->clearCanCommit();

        if (head_inst->isLoad() && head_inst->strictlyOrdered()) {
            DPRINTF(Commit, "[tid:%i] [sn:%llu] "
                    "Strictly ordered load, PC %s.\n",
                    tid, head_inst->seqNum, head_inst->pcState());
            toIEW->commitInfo[tid].strictlyOrdered = true;
            toIEW->commitInfo[tid].strictlyOrderedLoad = head_inst;
        } else {
            ++stats.commitNonSpecStalls;
        }

        return false;
    }

    // Check if the instruction caused a fault.  If so, trap.
    Fault inst_fault = head_inst->getFault();

    // hardware transactional memory
    // if a fault occurred within a HTM transaction
    // ensure that the transaction aborts
    if (inst_fault != NoFault && head_inst->inHtmTransactionalState()) {
        // There exists a generic HTM fault common to all ISAs
        if (!std::dynamic_pointer_cast<GenericHtmFailureFault>(inst_fault)) {
            DPRINTF(HtmCpu, "%s - fault (%s) encountered within transaction"
                            " - converting to GenericHtmFailureFault\n",
            head_inst->staticInst->getName(), inst_fault->name());
            inst_fault = std::make_shared<GenericHtmFailureFault>(
                head_inst->getHtmTransactionUid(),
                HtmFailureFaultCause::EXCEPTION);
        }
        // If this point is reached and the fault inherits from the HTM fault,
        // then there is no need to raise a new fault
    }

    // Stores mark themselves as completed.
    if (!head_inst->isStore() && inst_fault == NoFault) {
        head_inst->setCompleted();
    }

    if (inst_fault != NoFault) {
        DPRINTF(Commit,
                "Inst [tid:%i] [sn:%llu] PC %s has a %s fault. Runahead:%i, Poison:%i\n",
                tid, head_inst->seqNum, head_inst->pcState(), inst_fault->name(),
                head_inst->isRunahead(), head_inst->isPoisoned());

        if (iewStage->hasStoresToWB(tid) || inst_num > 0) {
            DPRINTF(Commit,
                    "[tid:%i] [sn:%llu] "
                    "Stores outstanding, fault must wait.\n",
                    tid, head_inst->seqNum);
            return false;
        }

        head_inst->setCompleted();

        // If instruction has faulted, let the checker execute it and
        // check if it sees the same fault and control flow.
        if (cpu->checker) {
            // Need to check the instruction before its fault is processed
            cpu->checker->verify(head_inst);
        }

        assert(!thread[tid]->noSquashFromTC);

        // Mark that we're in state update mode so that the trap's
        // execution doesn't generate extra squashes.
        thread[tid]->noSquashFromTC = true;

        /**
         * All runahead faults are ignored. The problem isn't "architecturally real",
         * and if it was a syscall, we definitely don't want it to execute speculatively.
         * The trap squash will still happen, but the trap itself does not execute
        */ 
        if (!head_inst->isRunahead()) {
            // Execute the trap.  Although it's slightly unrealistic in
            // terms of timing (as it doesn't wait for the full timing of
            // the trap event to complete before updating state), it's
            // needed to update the state as soon as possible.  This
            // prevents external agents from changing any specific state
            // that the trap need.
            cpu->trap(inst_fault, tid,
                    head_inst->notAnInst() ? nullStaticInstPtr :
                        head_inst->staticInst);
        } else {
            DPRINTF(RunaheadCommit,
                    "[tid:%i] [sn:%llu] %s fault ignored, inst is runahead\n",
                    tid, head_inst->seqNum, inst_fault->name());
        }

        // Exit state update mode to avoid accidental updating.
        thread[tid]->noSquashFromTC = false;

        commitStatus[tid] = TrapPending;

        DPRINTF(Commit,
            "[tid:%i] [sn:%llu] Committing instruction with fault\n",
            tid, head_inst->seqNum);
        if (head_inst->traceData) {
            // We ignore ReExecution "faults" here as they are not real
            // (architectural) faults but signal flush/replays.
            if (debug::ExecFaulting
                && dynamic_cast<ReExec*>(inst_fault.get()) == nullptr) {

                head_inst->traceData->setFaulting(true);
                head_inst->traceData->setFetchSeq(head_inst->seqNum);
                head_inst->traceData->setCPSeq(thread[tid]->numOp);
                head_inst->traceData->dump();
            }
            delete head_inst->traceData;
            head_inst->traceData = NULL;
        }

        // Generate trap squash event.
        generateTrapEvent(tid, inst_fault);
        return false;
    }

    updateComInstStats(head_inst);

    DPRINTF(Commit,
            "[tid:%i] [sn:%llu] Committing instruction with PC %s\n",
            tid, head_inst->seqNum, head_inst->pcState());
    if (head_inst->traceData) {
        head_inst->traceData->setFetchSeq(head_inst->seqNum);
        head_inst->traceData->setCPSeq(thread[tid]->numOp);
        head_inst->traceData->dump();
        delete head_inst->traceData;
        head_inst->traceData = NULL;
    }
    if (head_inst->isReturn()) {
        DPRINTF(Commit,
                "[tid:%i] [sn:%llu] Return Instruction Committed PC %s \n",
                tid, head_inst->seqNum, head_inst->pcState());
    }

    // Update the commit rename map
    // Runahead instructions don't update the map as the CPU is pseudoretiring, not really committing
    if (!head_inst->isRunahead()) {
        for (int i = 0; i < head_inst->numDestRegs(); i++)
            renameMap[tid]->setEntry(head_inst->flattenedDestIdx(i),
                                    head_inst->renamedDestIdx(i));
    } else if (head_inst->isPoisoned()) {
        // Sanity check
        for (int i = 0; i < head_inst->numDestRegs(); i++)
            assert(cpu->regPoisoned(head_inst->renamedDestIdx(i))
                   || head_inst->renamedDestIdx(i)->classValue() == InvalidRegClass
                   || head_inst->renamedDestIdx(i)->classValue() == MiscRegClass);
    }

    // hardware transactional memory
    // the HTM UID is purely for correctness and debugging purposes
    if (head_inst->isHtmStart())
        iewStage->setLastRetiredHtmUid(tid, head_inst->getHtmTransactionUid());

    // Finally clear the head ROB entry.
    rob->retireHead(tid);

    // If waiting for minimum work to be completed, check if we're done
    if (runaheadExitPolicy == REExitPolicy::MinimumWork &&
        runaheadExitable[tid] &&
        instsPseudoretired[tid] >= minRunaheadWork
        ) {
            DPRINTF(RunaheadCommit,
                    "[tid:%i] Exiting runahead because minimum work has been done.\n",
                    tid);
            exitRunahead[tid] = true;
            stats.runaheadExitCause[stats.REExitCause::MinWorkDone]++;
    }

#if TRACING_ON
    if (debug::O3PipeView) {
        head_inst->commitTick = curTick() - head_inst->fetchTick;
    }
#endif

    // If this was a store, record it for this cycle.
    if (head_inst->isStore() || head_inst->isAtomic())
        committedStores[tid] = true;

    // Return true to indicate that we have committed an instruction.
    return true;
}

void
Commit::getInsts()
{
    DPRINTF(Commit, "Getting instructions from Rename stage.\n");

    // Read any renamed instructions and place them into the ROB.
    int insts_to_process = std::min((int)renameWidth, fromRename->size);

    for (int inst_num = 0; inst_num < insts_to_process; ++inst_num) {
        const DynInstPtr &inst = fromRename->insts[inst_num];
        ThreadID tid = inst->threadNumber;

        if (!inst->isSquashed() &&
            commitStatus[tid] != ROBSquashing &&
            commitStatus[tid] != TrapPending) {
            changedROBNumEntries[tid] = true;

            DPRINTF(Commit, "[tid:%i] [sn:%llu] Inserting PC %s into ROB.\n",
                    tid, inst->seqNum, inst->pcState());

            rob->insertInst(inst);

            assert(rob->getThreadEntries(tid) <= rob->getMaxEntries(tid));

            youngestSeqNum[tid] = inst->seqNum;
        } else {
            DPRINTF(Commit, "[tid:%i] [sn:%llu] "
                    "Instruction PC %s was squashed, skipping.\n",
                    tid, inst->seqNum, inst->pcState());
        }
    }
}

void
Commit::markCompletedInsts()
{
    // Grab completed insts out of the IEW instruction queue, and mark
    // instructions completed within the ROB.
    for (int inst_num = 0; inst_num < fromIEW->size; ++inst_num) {
        assert(fromIEW->insts[inst_num]);
        if (!fromIEW->insts[inst_num]->isSquashed()) {
            DPRINTF(Commit, "[tid:%i] Marking PC %s, [sn:%llu] ready "
                    "within ROB.\n",
                    fromIEW->insts[inst_num]->threadNumber,
                    fromIEW->insts[inst_num]->pcState(),
                    fromIEW->insts[inst_num]->seqNum);

            // Mark the instruction as ready to commit.
            fromIEW->insts[inst_num]->setCanCommit();
        }
    }
}

void
Commit::updateComInstStats(const DynInstPtr &inst)
{
    ThreadID tid = inst->threadNumber;

    if (!inst->isMicroop() || inst->isLastMicroop()) {
        stats.instsCommitted[tid]++;
        if (!cpu->inRunahead(tid))
            instsBetweenRunahead[tid]++;

        if (inst->isRunahead()) {
            stats.instsPseudoretired[tid]++;
            instsPseudoretired[tid]++;

            if (inst->isPoisoned())
                ++stats.commitPoisonedInsts;
        }
    }
    stats.opsCommitted[tid]++;

    // To match the old model, don't count nops and instruction
    // prefetches towards the total commit count.
    if (!inst->isNop() && !inst->isInstPrefetch()) {
        cpu->instDone(tid, inst);
    }

    //
    //  Control Instructions
    //
    if (inst->isControl())
        stats.branches[tid]++;

    //
    //  Memory references
    //
    if (inst->isMemRef()) {
        stats.memRefs[tid]++;

        if (inst->isLoad()) {
            stats.loads[tid]++;
        }

        if (inst->isAtomic()) {
            stats.amos[tid]++;
        }
    }

    if (inst->isFullMemBarrier()) {
        stats.membars[tid]++;
    }

    // Integer Instruction
    if (inst->isInteger())
        stats.integer[tid]++;

    // Floating Point Instruction
    if (inst->isFloating())
        stats.floating[tid]++;
    // Vector Instruction
    if (inst->isVector())
        stats.vectorInstructions[tid]++;

    // Function Calls
    if (inst->isCall())
        stats.functionCalls[tid]++;

}

////////////////////////////////////////
//                                    //
//  SMT COMMIT POLICY MAINTAINED HERE //
//                                    //
////////////////////////////////////////
ThreadID
Commit::getCommittingThread()
{
    if (numThreads > 1) {
        switch (commitPolicy) {
          case CommitPolicy::RoundRobin:
            return roundRobin();

          case CommitPolicy::OldestReady:
            return oldestReady();

          default:
            return InvalidThreadID;
        }
    } else {
        assert(!activeThreads->empty());
        ThreadID tid = activeThreads->front();

        if (commitStatus[tid] == Running ||
            commitStatus[tid] == Idle ||
            commitStatus[tid] == FetchTrapPending) {
            return tid;
        } else {
            return InvalidThreadID;
        }
    }
}

ThreadID
Commit::roundRobin()
{
    std::list<ThreadID>::iterator pri_iter = priority_list.begin();
    std::list<ThreadID>::iterator end      = priority_list.end();

    while (pri_iter != end) {
        ThreadID tid = *pri_iter;

        if (commitStatus[tid] == Running ||
            commitStatus[tid] == Idle ||
            commitStatus[tid] == FetchTrapPending) {

            if (rob->isHeadReady(tid)) {
                priority_list.erase(pri_iter);
                priority_list.push_back(tid);

                return tid;
            }
        }

        pri_iter++;
    }

    return InvalidThreadID;
}

ThreadID
Commit::oldestReady()
{
    unsigned oldest = 0;
    unsigned oldest_seq_num = 0;
    bool first = true;

    std::list<ThreadID>::iterator threads = activeThreads->begin();
    std::list<ThreadID>::iterator end = activeThreads->end();

    while (threads != end) {
        ThreadID tid = *threads++;

        if (!rob->isEmpty(tid) &&
            (commitStatus[tid] == Running ||
             commitStatus[tid] == Idle ||
             commitStatus[tid] == FetchTrapPending)) {

            if (rob->isHeadReady(tid)) {

                const DynInstPtr &head_inst = rob->readHeadInst(tid);

                if (first) {
                    oldest = tid;
                    oldest_seq_num = head_inst->seqNum;
                    first = false;
                } else if (head_inst->seqNum < oldest_seq_num) {
                    oldest = tid;
                    oldest_seq_num = head_inst->seqNum;
                }
            }
        }
    }

    if (!first) {
        return oldest;
    } else {
        return InvalidThreadID;
    }
}

} // namespace runahead
} // namespace gem5
