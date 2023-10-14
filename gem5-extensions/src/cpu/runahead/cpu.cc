/*
 * Copyright (c) 2011-2012, 2014, 2016, 2017, 2019-2020 ARM Limited
 * Copyright (c) 2013 Advanced Micro Devices, Inc.
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
 * Copyright (c) 2011 Regents of the University of California
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

#include "cpu/runahead/cpu.hh"

#include "config/the_isa.hh"
#include "cpu/activity.hh"
#include "cpu/checker/cpu.hh"
#include "cpu/checker/thread_context.hh"
#include "cpu/runahead/dyn_inst.hh"
#include "cpu/runahead/limits.hh"
#include "cpu/runahead/thread_context.hh"
#include "cpu/simple_thread.hh"
#include "cpu/thread_context.hh"
#include "debug/Activity.hh"
#include "debug/Drain.hh"
#include "debug/RunaheadCPU.hh"
#include "debug/RunaheadCheckpoint.hh"
#include "debug/O3CPU.hh"
#include "debug/Quiesce.hh"
#include "enums/MemoryMode.hh"
#include "sim/cur_tick.hh"
#include "sim/full_system.hh"
#include "sim/process.hh"
#include "sim/stat_control.hh"
#include "sim/system.hh"

namespace gem5
{

struct BaseCPUParams;

namespace runahead
{

CPU::CPU(const BaseRunaheadCPUParams &params)
    : BaseCPU(params),
      mmu(params.mmu),
      tickEvent([this]{ tick(); }, "RunaheadCPU tick",
                false, Event::CPU_Tick_Pri),
      threadExitEvent([this]{ exitThreads(); }, "RunaheadCPU exit threads",
                false, Event::CPU_Exit_Pri),
      runaheadEnabled(params.enableRunahead),
      runaheadInFlightThreshold(params.runaheadInFlightThreshold),
      allowOverlappingRunahead(params.allowOverlappingRunahead),
      lllDepthThreshold(params.lllDepthThreshold),
#ifndef NDEBUG
      instcount(0),
#endif
      removeInstsThisCycle(false),
      fetch(this, params),
      decode(this, params),
      rename(this, params),
      iew(this, params),
      commit(this, params),

      regFile(params.numPhysIntRegs,
              params.numPhysFloatRegs,
              params.numPhysVecRegs,
              params.numPhysVecPredRegs,
              params.numPhysCCRegs,
              params.isa[0]->regClasses()),
    
      freeList(name() + ".freelist", &regFile),

      rob(this, params),

      // TODO? revisit RE cache block size
      runaheadCache(this, params.runaheadCacheSize, 8),

      scoreboard(name() + ".scoreboard", regFile.totalNumPhysRegs()),

      isa(numThreads, NULL),

      archStateCheckpoint(this, params),

      timeBuffer(params.backComSize, params.forwardComSize),
      fetchQueue(params.backComSize, params.forwardComSize),
      decodeQueue(params.backComSize, params.forwardComSize),
      renameQueue(params.backComSize, params.forwardComSize),
      iewQueue(params.backComSize, params.forwardComSize),
      activityRec(name(), NumStages,
                  params.backComSize + params.forwardComSize,
                  params.activity),

      globalSeqNum(1),
      system(params.system),
      lastRunningCycle(curCycle()),
      cpuStats(this)
{
    fatal_if(FullSystem && params.numThreads > 1,
            "SMT is not supported in Runahead in full system mode currently.");

    fatal_if(!FullSystem && params.numThreads < params.workload.size(),
            "More workload items (%d) than threads (%d) on CPU %s.",
            params.workload.size(), params.numThreads, name());

    if (!params.switched_out) {
        _status = Running;
    } else {
        _status = SwitchedOut;
    }

    if (params.checker) {
        BaseCPU *temp_checker = params.checker;
        checker = dynamic_cast<Checker<DynInstPtr> *>(temp_checker);
        checker->setIcachePort(&fetch.getInstPort());
        checker->setSystem(params.system);
    } else {
        checker = NULL;
    }

    if (!FullSystem) {
        thread.resize(numThreads);
        tids.resize(numThreads);
    }

    // The stages also need their CPU pointer setup.  However this
    // must be done at the upper level CPU because they have pointers
    // to the upper level CPU, and not this CPU.

    // Set up Pointers to the activeThreads list for each stage
    fetch.setActiveThreads(&activeThreads);
    decode.setActiveThreads(&activeThreads);
    rename.setActiveThreads(&activeThreads);
    iew.setActiveThreads(&activeThreads);
    commit.setActiveThreads(&activeThreads);

    // Give each of the stages the time buffer they will use.
    fetch.setTimeBuffer(&timeBuffer);
    decode.setTimeBuffer(&timeBuffer);
    rename.setTimeBuffer(&timeBuffer);
    iew.setTimeBuffer(&timeBuffer);
    commit.setTimeBuffer(&timeBuffer);

    // Also setup each of the stages' queues.
    fetch.setFetchQueue(&fetchQueue);
    decode.setFetchQueue(&fetchQueue);
    commit.setFetchQueue(&fetchQueue);
    decode.setDecodeQueue(&decodeQueue);
    rename.setDecodeQueue(&decodeQueue);
    rename.setRenameQueue(&renameQueue);
    iew.setRenameQueue(&renameQueue);
    iew.setIEWQueue(&iewQueue);
    commit.setIEWQueue(&iewQueue);
    commit.setRenameQueue(&renameQueue);

    commit.setIEWStage(&iew);
    rename.setIEWStage(&iew);
    rename.setCommitStage(&commit);

    // Setup the runahead cache for IEW
    // IEW will passthrough down to the individual LSQ units that need it
    iew.setRunaheadCache(&runaheadCache);

    ThreadID active_threads;
    if (FullSystem) {
        active_threads = 1;
    } else {
        active_threads = params.workload.size();

        if (active_threads > MaxThreads) {
            panic("Workload Size too large. Increase the 'MaxThreads' "
                  "constant in cpu/runahead/limits.hh or edit your workload size.");
        }
    }

    // Make Sure That this a Valid Architeture
    assert(numThreads);
    const auto &regClasses = params.isa[0]->regClasses();

    assert(params.numPhysIntRegs >=
            numThreads * regClasses.at(IntRegClass).numRegs());
    assert(params.numPhysFloatRegs >=
            numThreads * regClasses.at(FloatRegClass).numRegs());
    assert(params.numPhysVecRegs >=
            numThreads * regClasses.at(VecRegClass).numRegs());
    assert(params.numPhysVecPredRegs >=
            numThreads * regClasses.at(VecPredRegClass).numRegs());
    assert(params.numPhysCCRegs >=
            numThreads * regClasses.at(CCRegClass).numRegs());

    // Just make this a warning and go ahead anyway, to keep from having to
    // add checks everywhere.
    warn_if(regClasses.at(CCRegClass).numRegs() == 0 &&
            params.numPhysCCRegs != 0,
            "Non-zero number of physical CC regs specified, even though\n"
            "    ISA does not use them.");

    rename.setScoreboard(&scoreboard);
    iew.setScoreboard(&scoreboard);

    // Setup the rename map for whichever stages need it.
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        isa[tid] = dynamic_cast<TheISA::ISA *>(params.isa[tid]);
        commitRenameMap[tid].init(regClasses, &regFile, &freeList);
        renameMap[tid].init(regClasses, &regFile, &freeList);
    }

    // Initialize rename map to assign physical registers to the
    // architectural registers for active threads only.
    for (ThreadID tid = 0; tid < active_threads; tid++) {
        for (auto type = (RegClassType)0; type <= CCRegClass;
                type = (RegClassType)(type + 1)) {
            for (RegIndex ridx = 0; ridx < regClasses.at(type).numRegs();
                    ++ridx) {
                // Note that we can't use the rename() method because we don't
                // want special treatment for the zero register at this point
                RegId rid = RegId(type, ridx);
                PhysRegIdPtr phys_reg = freeList.getReg(type);
                renameMap[tid].setEntry(rid, phys_reg);
                commitRenameMap[tid].setEntry(rid, phys_reg);
            }
        }
    }

    rename.setRenameMap(renameMap);
    commit.setRenameMap(commitRenameMap);
    rename.setFreeList(&freeList);

    // Setup the ROB for whichever stages need it.
    commit.setROB(&rob);
    rename.setROB(&rob);

    lastActivatedCycle = 0;

    DPRINTF(RunaheadCPU, "Creating RunaheadCPU object.\n");

    // Setup any thread state.
    thread.resize(numThreads);

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        if (FullSystem) {
            // SMT is not supported in FS mode yet.
            assert(numThreads == 1);
            thread[tid] = new ThreadState(this, 0, NULL);
        } else {
            if (tid < params.workload.size()) {
                DPRINTF(O3CPU, "Workload[%i] process is %#x", tid,
                        thread[tid]);
                thread[tid] = new ThreadState(this, tid, params.workload[tid]);
            } else {
                //Allocate Empty thread so M5 can use later
                //when scheduling threads to CPU
                Process* dummy_proc = NULL;

                thread[tid] = new ThreadState(this, tid, dummy_proc);
            }
        }

        gem5::ThreadContext *tc;

        // Setup the TC that will serve as the interface to the threads/CPU.
        auto *runahead_tc = new ThreadContext;

        tc = runahead_tc;

        // If we're using a checker, then the TC should be the
        // CheckerThreadContext.
        if (params.checker) {
            tc = new CheckerThreadContext<ThreadContext>(runahead_tc, checker);
        }

        runahead_tc->cpu = this;
        runahead_tc->thread = thread[tid];

        // Give the thread the TC.
        thread[tid]->tc = tc;

        // Add the TC to the CPU's list of TC's.
        threadContexts.push_back(tc);
    }

    // RunaheadCPU always requires an interrupt controller.
    if (!params.switched_out && interrupts.empty()) {
        fatal("RunaheadCPU %s has no interrupt controller.\n"
              "Ensure createInterruptController() is called.\n", name());
    }
}

void
CPU::regProbePoints()
{
    BaseCPU::regProbePoints();

    ppInstAccessComplete = new ProbePointArg<PacketPtr>(
            getProbeManager(), "InstAccessComplete");
    ppDataAccessComplete = new ProbePointArg<
        std::pair<DynInstPtr, PacketPtr>>(
                getProbeManager(), "DataAccessComplete");

    fetch.regProbePoints();
    rename.regProbePoints();
    iew.regProbePoints();
    commit.regProbePoints();
}

CPU::CPUStats::CPUStats(CPU *cpu)
    : statistics::Group(cpu),
      ADD_STAT(timesIdled, statistics::units::Count::get(),
               "Number of times that the entire CPU went into an idle state "
               "and unscheduled itself"),
      ADD_STAT(idleCycles, statistics::units::Cycle::get(),
               "Total number of cycles that the CPU has spent unscheduled due "
               "to idling"),
      ADD_STAT(quiesceCycles, statistics::units::Cycle::get(),
               "Total number of cycles that CPU has spent quiesced or waiting "
               "for an interrupt"),
      ADD_STAT(committedInsts, statistics::units::Count::get(),
               "Number of Instructions Simulated"),
      ADD_STAT(committedOps, statistics::units::Count::get(),
               "Number of Ops (including micro ops) Simulated"),
      ADD_STAT(cpi, statistics::units::Rate<
                    statistics::units::Cycle, statistics::units::Count>::get(),
               "CPI: Cycles Per Instruction"),
      ADD_STAT(totalCpi, statistics::units::Rate<
                    statistics::units::Cycle, statistics::units::Count>::get(),
               "CPI: Total CPI of All Threads"),
      ADD_STAT(ipc, statistics::units::Rate<
                    statistics::units::Count, statistics::units::Cycle>::get(),
               "IPC: Instructions Per Cycle"),
      ADD_STAT(totalIpc, statistics::units::Rate<
                    statistics::units::Count, statistics::units::Cycle>::get(),
               "IPC: Total IPC of All Threads"),
      ADD_STAT(intRegfileReads, statistics::units::Count::get(),
               "Number of integer regfile reads"),
      ADD_STAT(intRegfileWrites, statistics::units::Count::get(),
               "Number of integer regfile writes"),
      ADD_STAT(fpRegfileReads, statistics::units::Count::get(),
               "Number of floating regfile reads"),
      ADD_STAT(fpRegfileWrites, statistics::units::Count::get(),
               "Number of floating regfile writes"),
      ADD_STAT(vecRegfileReads, statistics::units::Count::get(),
               "number of vector regfile reads"),
      ADD_STAT(vecRegfileWrites, statistics::units::Count::get(),
               "number of vector regfile writes"),
      ADD_STAT(vecPredRegfileReads, statistics::units::Count::get(),
               "number of predicate regfile reads"),
      ADD_STAT(vecPredRegfileWrites, statistics::units::Count::get(),
               "number of predicate regfile writes"),
      ADD_STAT(ccRegfileReads, statistics::units::Count::get(),
               "number of cc regfile reads"),
      ADD_STAT(ccRegfileWrites, statistics::units::Count::get(),
               "number of cc regfile writes"),
      ADD_STAT(miscRegfileReads, statistics::units::Count::get(),
               "number of misc regfile reads"),
      ADD_STAT(miscRegfileWrites, statistics::units::Count::get(),
               "number of misc regfile writes"),
      ADD_STAT(runaheadPeriods, statistics::units::Count::get(),
               "Amount of times runahead was entered"),
      ADD_STAT(runaheadCycles, statistics::units::Cycle::get(),
               "Amount of cycles spent in runahead mode"),
      ADD_STAT(refusedRunaheadEntries, statistics::units::Count::get(),
               "Amount of times the CPU refused to enter into runahead"),
      ADD_STAT(instsPseudoRetiredPerPeriod, statistics::units::Count::get(),
               "Amount of instructions pseudoretired by runahead execution periods"),
      ADD_STAT(instsFetchedBetweenRunahead, statistics::units::Count::get(),
               "Amount of instructions fetched between runahead periods"),
      ADD_STAT(instsRetiredBetweenRunahead, statistics::units::Count::get(),
               "Amount of instructions retired between runahead periods"),
      ADD_STAT(triggerLLLinFlightCycles, statistics::units::Cycle::get(),
               "Amount of cycles a load has been in-flight when it triggered runahead"),
      ADD_STAT(intRegPoisoned, statistics::units::Count::get(),
               "Amount of times an integer register was marked as poisoned"),
      ADD_STAT(intRegCured, statistics::units::Count::get(),
               "Amount of times an integer register's poison was reset in runahead"),
      ADD_STAT(floatRegPoisoned, statistics::units::Count::get(),
               "Amount of times a float register was marked as poisoned"),
      ADD_STAT(floatRegCured, statistics::units::Count::get(),
               "Amount of times a float register's poison was reset in runahead"),
      ADD_STAT(vecRegPoisoned, statistics::units::Count::get(),
               "Amount of times a vector register was marked as poisoned"),
      ADD_STAT(vecRegCured, statistics::units::Count::get(),
               "Amount of times a vector register's poison was reset in runahead"),
      ADD_STAT(vecPredRegPoisoned, statistics::units::Count::get(),
               "Amount of times a predicate register was marked as poisoned"),
      ADD_STAT(vecPredRegCured, statistics::units::Count::get(),
               "Amount of times a predicate register's poison was reset in runahead"),
      ADD_STAT(ccRegPoisoned, statistics::units::Count::get(),
               "Amount of times a CC register was marked as poisoned"),
      ADD_STAT(ccRegCured, statistics::units::Count::get(),
               "Amount of times a CC register's poison was reset in runahead"),
      ADD_STAT(miscRegPoisoned, statistics::units::Count::get(),
               "Amount of times a misc register was marked as poisoned"),
      ADD_STAT(miscRegCured, statistics::units::Count::get(),
               "Amount of times a misc register's poison was reset in runahead")
{
    // Register any of the RunaheadCPU's stats here.
    timesIdled
        .prereq(timesIdled);

    idleCycles
        .prereq(idleCycles);

    quiesceCycles
        .prereq(quiesceCycles);

    // Number of Instructions simulated
    // --------------------------------
    // Should probably be in Base CPU but need templated
    // MaxThreads so put in here instead
    committedInsts
        .init(cpu->numThreads)
        .flags(statistics::total);

    committedOps
        .init(cpu->numThreads)
        .flags(statistics::total);

    cpi
        .precision(6);
    cpi = cpu->baseStats.numCycles / committedInsts;

    totalCpi
        .precision(6);
    totalCpi = cpu->baseStats.numCycles / sum(committedInsts);

    ipc
        .precision(6);
    ipc = committedInsts / cpu->baseStats.numCycles;

    totalIpc
        .precision(6);
    totalIpc = sum(committedInsts) / cpu->baseStats.numCycles;

    intRegfileReads
        .prereq(intRegfileReads);

    intRegfileWrites
        .prereq(intRegfileWrites);

    fpRegfileReads
        .prereq(fpRegfileReads);

    fpRegfileWrites
        .prereq(fpRegfileWrites);

    vecRegfileReads
        .prereq(vecRegfileReads);

    vecRegfileWrites
        .prereq(vecRegfileWrites);

    vecPredRegfileReads
        .prereq(vecPredRegfileReads);

    vecPredRegfileWrites
        .prereq(vecPredRegfileWrites);

    ccRegfileReads
        .prereq(ccRegfileReads);

    ccRegfileWrites
        .prereq(ccRegfileWrites);

    miscRegfileReads
        .prereq(miscRegfileReads);

    miscRegfileWrites
        .prereq(miscRegfileWrites);
    
    runaheadPeriods
        .prereq(runaheadPeriods);

    runaheadCycles
        .init(0, 1000, 50)
        .flags(statistics::total);

    refusedRunaheadEntries
        .init(OverlappingPeriod + 1)
        .flags(statistics::total);

    instsPseudoRetiredPerPeriod
        .init(12)
        .flags(statistics::total);

    instsFetchedBetweenRunahead
        .init(0, 2000, 100)
        .flags(statistics::total);

    instsRetiredBetweenRunahead
        .init(0, 1000, 50)
        .flags(statistics::total);

    triggerLLLinFlightCycles
        .init(8)
        .flags(statistics::total);

    intRegPoisoned
        .prereq(intRegPoisoned);
    
    intRegCured
        .prereq(intRegCured);

    floatRegPoisoned
        .prereq(floatRegPoisoned);
    
    floatRegCured
        .prereq(floatRegCured);

    vecRegPoisoned
        .prereq(vecRegPoisoned);
    
    vecRegCured
        .prereq(vecRegCured);

    vecPredRegPoisoned
        .prereq(vecPredRegPoisoned);

    vecPredRegCured
        .prereq(vecPredRegCured);

    ccRegPoisoned
        .prereq(ccRegPoisoned);
    
    ccRegCured
        .prereq(ccRegCured);

    miscRegPoisoned
        .prereq(miscRegPoisoned);
    
    miscRegCured
        .prereq(miscRegCured);
}

void
CPU::tick()
{
    offTick = false;

    DPRINTF(O3CPU, "\n\n===== TICK STARTS =====\nRunaheadCPU: Ticking main, RunaheadCPU.\n");
    assert(!switchedOut());
    assert(drainState() != DrainState::Drained);

    ++baseStats.numCycles;
    updateCycleCounters(BaseCPU::CPU_STATE_ON);
    
//    activity = false;

    //Tick each of the stages
    fetch.tick();

    decode.tick();

    rename.tick();

    iew.tick();

    commit.tick();

    // Now advance the time buffers
    timeBuffer.advance();

    fetchQueue.advance();
    decodeQueue.advance();
    renameQueue.advance();
    iewQueue.advance();

    activityRec.advance();

    if (removeInstsThisCycle) {
        cleanUpRemovedInsts();
    }

    if (!tickEvent.scheduled()) {
        if (_status == SwitchedOut) {
            DPRINTF(O3CPU, "Switched out!\n");
            // increment stat
            lastRunningCycle = curCycle();
        } else if (!activityRec.active() || _status == Idle) {
            DPRINTF(O3CPU, "Idle!\n");
            lastRunningCycle = curCycle();
            cpuStats.timesIdled++;
        } else {
            DPRINTF(O3CPU, "Scheduling next tick!\n");
            schedule(tickEvent, clockEdge(Cycles(1)));
        }
    }

    if (!FullSystem)
        updateThreadPriority();

    tryDrain();

    DPRINTF(O3CPU, "\n===== TICK ENDS =====\n\n");
    offTick = true;
}

void
CPU::init()
{
    BaseCPU::init();

    for (ThreadID tid = 0; tid < numThreads; ++tid) {
        // Set noSquashFromTC so that the CPU doesn't squash when initially
        // setting up registers.
        thread[tid]->noSquashFromTC = true;
    }

    // Clear noSquashFromTC.
    for (int tid = 0; tid < numThreads; ++tid)
        thread[tid]->noSquashFromTC = false;

    commit.setThreads(thread);
}

void
CPU::startup()
{
    BaseCPU::startup();

    fetch.startupStage();
    decode.startupStage();
    iew.startupStage();
    rename.startupStage();
    commit.startupStage();
}

void
CPU::activateThread(ThreadID tid)
{
    std::list<ThreadID>::iterator isActive =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%i] Calling activate thread.\n", tid);
    assert(!switchedOut());

    if (isActive == activeThreads.end()) {
        DPRINTF(O3CPU, "[tid:%i] Adding to active threads list\n", tid);

        activeThreads.push_back(tid);
    }
}

void
CPU::deactivateThread(ThreadID tid)
{
    // hardware transactional memory
    // shouldn't deactivate thread in the middle of a transaction
    assert(!commit.executingHtmTransaction(tid));

    //Remove From Active List, if Active
    std::list<ThreadID>::iterator thread_it =
        std::find(activeThreads.begin(), activeThreads.end(), tid);

    DPRINTF(O3CPU, "[tid:%i] Calling deactivate thread.\n", tid);
    assert(!switchedOut());

    if (thread_it != activeThreads.end()) {
        DPRINTF(O3CPU,"[tid:%i] Removing from active threads list\n",
                tid);
        activeThreads.erase(thread_it);
    }

    fetch.deactivateThread(tid);
    commit.deactivateThread(tid);
}

Counter
CPU::totalInsts() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numInst;

    return total;
}

Counter
CPU::totalOps() const
{
    Counter total(0);

    ThreadID size = thread.size();
    for (ThreadID i = 0; i < size; i++)
        total += thread[i]->numOp;

    return total;
}

void
CPU::activateContext(ThreadID tid)
{
    assert(!switchedOut());

    // Needs to set each stage to running as well.
    activateThread(tid);

    // We don't want to wake the CPU if it is drained. In that case,
    // we just want to flag the thread as active and schedule the tick
    // event from drainResume() instead.
    if (drainState() == DrainState::Drained)
        return;

    // If we are time 0 or if the last activation time is in the past,
    // schedule the next tick and wake up the fetch unit
    if (lastActivatedCycle == 0 || lastActivatedCycle < curTick()) {
        scheduleTickEvent(Cycles(0));

        // Be sure to signal that there's some activity so the CPU doesn't
        // deschedule itself.
        activityRec.activity();
        fetch.wakeFromQuiesce();

        Cycles cycles(curCycle() - lastRunningCycle);
        // @todo: This is an oddity that is only here to match the stats
        if (cycles != 0)
            --cycles;
        cpuStats.quiesceCycles += cycles;

        lastActivatedCycle = curTick();

        _status = Running;

        BaseCPU::activateContext(tid);
    }
}

void
CPU::suspendContext(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Suspending Thread Context.\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);

    // If this was the last thread then unschedule the tick event.
    if (activeThreads.size() == 0) {
        unscheduleTickEvent();
        lastRunningCycle = curCycle();
        _status = Idle;
    }

    DPRINTF(Quiesce, "Suspending Context\n");

    BaseCPU::suspendContext(tid);
}

void
CPU::haltContext(ThreadID tid)
{
    //For now, this is the same as deallocate
    DPRINTF(O3CPU,"[tid:%i] Halt Context called. Deallocating\n", tid);
    assert(!switchedOut());

    deactivateThread(tid);
    removeThread(tid);

    // If this was the last thread then unschedule the tick event.
    if (activeThreads.size() == 0) {
        if (tickEvent.scheduled())
        {
            unscheduleTickEvent();
        }
        lastRunningCycle = curCycle();
        _status = Idle;
    }
    updateCycleCounters(BaseCPU::CPU_STATE_SLEEP);
}

void
CPU::insertThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Initializing thread into CPU");
    // Will change now that the PC and thread state is internal to the CPU
    // and not in the ThreadContext.
    gem5::ThreadContext *src_tc;
    if (FullSystem)
        src_tc = system->threads[tid];
    else
        src_tc = tcBase(tid);

    //Bind Int Regs to Rename Map
    const auto &regClasses = isa[tid]->regClasses();

    for (auto type = (RegClassType)0; type <= CCRegClass;
            type = (RegClassType)(type + 1)) {
        for (RegIndex idx = 0; idx < regClasses.at(type).numRegs(); idx++) {
            PhysRegIdPtr phys_reg = freeList.getReg(type);
            renameMap[tid].setEntry(RegId(type, idx), phys_reg);
            scoreboard.setReg(phys_reg);
        }
    }

    //Copy Thread Data Into RegFile
    //copyFromTC(tid);

    //Set PC/NPC/NNPC
    pcState(src_tc->pcState(), tid);

    src_tc->setStatus(gem5::ThreadContext::Active);

    activateContext(tid);

    //Reset ROB/IQ/LSQ Entries
    commit.rob->resetEntries();
}

void
CPU::removeThread(ThreadID tid)
{
    DPRINTF(O3CPU,"[tid:%i] Removing thread context from CPU.\n", tid);

    // Copy Thread Data From RegFile
    // If thread is suspended, it might be re-allocated
    // copyToTC(tid);


    // @todo: 2-27-2008: Fix how we free up rename mappings
    // here to alleviate the case for double-freeing registers
    // in SMT workloads.

    // clear all thread-specific states in each stage of the pipeline
    // since this thread is going to be completely removed from the CPU
    commit.clearStates(tid);
    fetch.clearStates(tid);
    decode.clearStates(tid);
    rename.clearStates(tid);
    iew.clearStates(tid);

    // Flush out any old data from the time buffers.
    for (int i = 0; i < timeBuffer.getSize(); ++i) {
        timeBuffer.advance();
        fetchQueue.advance();
        decodeQueue.advance();
        renameQueue.advance();
        iewQueue.advance();
    }

    // at this step, all instructions in the pipeline should be already
    // either committed successfully or squashed. All thread-specific
    // queues in the pipeline must be empty.
    assert(iew.instQueue.getCount(tid) == 0);
    assert(iew.ldstQueue.getCount(tid) == 0);
    assert(commit.rob->isEmpty(tid));

    // Reset ROB/IQ/LSQ Entries

    // Commented out for now.  This should be possible to do by
    // telling all the pipeline stages to drain first, and then
    // checking until the drain completes.  Once the pipeline is
    // drained, call resetEntries(). - 10-09-06 ktlim
/*
    if (activeThreads.size() >= 1) {
        commit.rob->resetEntries();
        iew.resetEntries();
    }
*/
}

Fault
CPU::getInterrupts()
{
    // Check if there are any outstanding interrupts
    return interrupts[0]->getInterrupt();
}

void
CPU::processInterrupts(const Fault &interrupt)
{
    // Check for interrupts here.  For now can copy the code that
    // exists within isa_fullsys_traits.hh.  Also assume that thread 0
    // is the one that handles the interrupts.
    // @todo: Possibly consolidate the interrupt checking code.
    // @todo: Allow other threads to handle interrupts.

    assert(interrupt != NoFault);
    interrupts[0]->updateIntrInfo();

    DPRINTF(O3CPU, "Interrupt %s being handled\n", interrupt->name());
    trap(interrupt, 0, nullptr);
}

void
CPU::trap(const Fault &fault, ThreadID tid, const StaticInstPtr &inst)
{
    // Pass the thread's TC into the invoke method.
    fault->invoke(threadContexts[tid], inst);
}

void
CPU::serializeThread(CheckpointOut &cp, ThreadID tid) const
{
    thread[tid]->serialize(cp);
}

void
CPU::unserializeThread(CheckpointIn &cp, ThreadID tid)
{
    thread[tid]->unserialize(cp);
}

DrainState
CPU::drain()
{
    // Deschedule any power gating event (if any)
    deschedulePowerGatingEvent();

    // If the CPU isn't doing anything, then return immediately.
    if (switchedOut())
        return DrainState::Drained;

    DPRINTF(Drain, "Draining...\n");

    // We only need to signal a drain to the commit stage as this
    // initiates squashing controls the draining. Once the commit
    // stage commits an instruction where it is safe to stop, it'll
    // squash the rest of the instructions in the pipeline and force
    // the fetch stage to stall. The pipeline will be drained once all
    // in-flight instructions have retired.
    commit.drain();

    // Wake the CPU and record activity so everything can drain out if
    // the CPU was not able to immediately drain.
    if (!isCpuDrained())  {
        // If a thread is suspended, wake it up so it can be drained
        for (auto t : threadContexts) {
            if (t->status() == gem5::ThreadContext::Suspended){
                DPRINTF(Drain, "Currently suspended so activate %i \n",
                        t->threadId());
                t->activate();
                // As the thread is now active, change the power state as well
                activateContext(t->threadId());
            }
        }

        wakeCPU();
        activityRec.activity();

        DPRINTF(Drain, "CPU not drained\n");

        return DrainState::Draining;
    } else {
        DPRINTF(Drain, "CPU is already drained\n");
        if (tickEvent.scheduled())
            deschedule(tickEvent);

        // Flush out any old data from the time buffers.  In
        // particular, there might be some data in flight from the
        // fetch stage that isn't visible in any of the CPU buffers we
        // test in isCpuDrained().
        for (int i = 0; i < timeBuffer.getSize(); ++i) {
            timeBuffer.advance();
            fetchQueue.advance();
            decodeQueue.advance();
            renameQueue.advance();
            iewQueue.advance();
        }

        drainSanityCheck();
        return DrainState::Drained;
    }
}

bool
CPU::tryDrain()
{
    if (drainState() != DrainState::Draining || !isCpuDrained())
        return false;

    if (tickEvent.scheduled())
        deschedule(tickEvent);

    DPRINTF(Drain, "CPU done draining, processing drain event\n");
    signalDrainDone();

    return true;
}

void
CPU::drainSanityCheck() const
{
    assert(isCpuDrained());
    fetch.drainSanityCheck();
    decode.drainSanityCheck();
    rename.drainSanityCheck();
    iew.drainSanityCheck();
    commit.drainSanityCheck();
}

bool
CPU::isCpuDrained() const
{
    bool drained(true);

    if (!instList.empty() || !removeList.empty()) {
        DPRINTF(Drain, "Main CPU structures not drained.\n");
        drained = false;
    }

    if (!fetch.isDrained()) {
        DPRINTF(Drain, "Fetch not drained.\n");
        drained = false;
    }

    if (!decode.isDrained()) {
        DPRINTF(Drain, "Decode not drained.\n");
        drained = false;
    }

    if (!rename.isDrained()) {
        DPRINTF(Drain, "Rename not drained.\n");
        drained = false;
    }

    if (!iew.isDrained()) {
        DPRINTF(Drain, "IEW not drained.\n");
        drained = false;
    }

    if (!commit.isDrained()) {
        DPRINTF(Drain, "Commit not drained.\n");
        drained = false;
    }

    return drained;
}

void CPU::commitDrained(ThreadID tid) { fetch.drainStall(tid); }

void
CPU::drainResume()
{
    if (switchedOut())
        return;

    DPRINTF(Drain, "Resuming...\n");
    verifyMemoryMode();

    fetch.drainResume();
    commit.drainResume();

    _status = Idle;
    for (ThreadID i = 0; i < thread.size(); i++) {
        if (thread[i]->status() == gem5::ThreadContext::Active) {
            DPRINTF(Drain, "Activating thread: %i\n", i);
            activateThread(i);
            _status = Running;
        }
    }

    assert(!tickEvent.scheduled());
    if (_status == Running)
        schedule(tickEvent, nextCycle());

    // Reschedule any power gating event (if any)
    schedulePowerGatingEvent();
}

void
CPU::switchOut()
{
    DPRINTF(O3CPU, "Switching out\n");
    BaseCPU::switchOut();

    activityRec.reset();

    _status = SwitchedOut;

    if (checker)
        checker->switchOut();
}

void
CPU::takeOverFrom(BaseCPU *oldCPU)
{
    BaseCPU::takeOverFrom(oldCPU);

    fetch.takeOverFrom();
    decode.takeOverFrom();
    rename.takeOverFrom();
    iew.takeOverFrom();
    commit.takeOverFrom();

    assert(!tickEvent.scheduled());

    auto *oldRunaheadCPU = dynamic_cast<CPU *>(oldCPU);
    if (oldRunaheadCPU)
        globalSeqNum = oldRunaheadCPU->globalSeqNum;

    lastRunningCycle = curCycle();
    _status = Idle;
}

void
CPU::verifyMemoryMode() const
{
    if (!system->isTimingMode()) {
        fatal("The Runahead CPU requires the memory system to be in "
              "'timing' mode.\n");
    }
}

RegVal
CPU::readMiscRegNoEffect(int misc_reg, ThreadID tid) const
{
    RegVal val = isa[tid]->readMiscRegNoEffect(misc_reg);
    // NSE = No Side-Effect
    DPRINTF(O3CPU, "[NSE] access to misc reg %i, has data %#x\n",
            misc_reg, val);
    return val;
}

RegVal
CPU::readMiscReg(int misc_reg, ThreadID tid)
{
    cpuStats.miscRegfileReads++;
    RegVal val = isa[tid]->readMiscReg(misc_reg);
    DPRINTF(O3CPU, "Access to misc reg %i, has data %#x\n",
            misc_reg, val);
    return val;
}

void
CPU::setMiscRegNoEffect(int misc_reg, RegVal val, ThreadID tid)
{
    DPRINTF(O3CPU, "[NSE] Setting misc reg %i to %#x\n",
            misc_reg, val);
    isa[tid]->setMiscRegNoEffect(misc_reg, val);
}

void
CPU::setMiscReg(int misc_reg, RegVal val, ThreadID tid)
{
    cpuStats.miscRegfileWrites++;
    DPRINTF(O3CPU, "Setting misc reg %i to %#x\n",
            misc_reg, val);
    isa[tid]->setMiscReg(misc_reg, val);
}

RegVal
CPU::getReg(PhysRegIdPtr phys_reg)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileReads++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileReads++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileReads++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileReads++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileReads++;
        break;
      default:
        break;
    }
    return regFile.getReg(phys_reg);
}

void
CPU::getReg(PhysRegIdPtr phys_reg, void *val)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileReads++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileReads++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileReads++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileReads++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileReads++;
        break;
      default:
        break;
    }
    regFile.getReg(phys_reg, val);
}

void *
CPU::getWritableReg(PhysRegIdPtr phys_reg)
{
    switch (phys_reg->classValue()) {
      case VecRegClass:
        cpuStats.vecRegfileReads++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileReads++;
        break;
      default:
        break;
    }
    return regFile.getWritableReg(phys_reg);
}

void
CPU::setReg(PhysRegIdPtr phys_reg, RegVal val)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileWrites++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileWrites++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileWrites++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileWrites++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileWrites++;
        break;
      default:
        break;
    }
    regFile.setReg(phys_reg, val);
}

void
CPU::setReg(PhysRegIdPtr phys_reg, const void *val)
{
    switch (phys_reg->classValue()) {
      case IntRegClass:
        cpuStats.intRegfileWrites++;
        break;
      case FloatRegClass:
        cpuStats.fpRegfileWrites++;
        break;
      case CCRegClass:
        cpuStats.ccRegfileWrites++;
        break;
      case VecRegClass:
      case VecElemClass:
        cpuStats.vecRegfileWrites++;
        break;
      case VecPredRegClass:
        cpuStats.vecPredRegfileWrites++;
        break;
      default:
        break;
    }
    regFile.setReg(phys_reg, val);
}

RegVal
CPU::getArchReg(const RegId &reg, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    return regFile.getReg(phys_reg);
}

void
CPU::getArchReg(const RegId &reg, void *val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    regFile.getReg(phys_reg, val);
}

void *
CPU::getWritableArchReg(const RegId &reg, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    return regFile.getWritableReg(phys_reg);
}

void
CPU::setArchReg(const RegId &reg, RegVal val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    regFile.setReg(phys_reg, val);
}

void
CPU::setArchReg(const RegId &reg, const void *val, ThreadID tid)
{
    PhysRegIdPtr phys_reg = commitRenameMap[tid].lookup(reg);
    regFile.setReg(phys_reg, val);
}

const PCStateBase &
CPU::pcState(ThreadID tid)
{
    return commit.pcState(tid);
}

void
CPU::pcState(const PCStateBase &val, ThreadID tid)
{
    commit.pcState(val, tid);
}

void
CPU::squashFromTC(ThreadID tid)
{
    thread[tid]->noSquashFromTC = true;
    commit.generateTCEvent(tid);
}

CPU::ListIt
CPU::addInst(const DynInstPtr &inst)
{
    instList.push_back(inst);

    return --(instList.end());
}

void
CPU::instDone(ThreadID tid, const DynInstPtr &inst)
{
    // Keep an instruction count.
    if (!inst->isMicroop() || inst->isLastMicroop()) {
        thread[tid]->numInst++;
        thread[tid]->threadStats.numInsts++;
        cpuStats.committedInsts[tid]++;

        // Check for instruction-count-based events.
        thread[tid]->comInstEventQueue.serviceEvents(thread[tid]->numInst);
    }
    thread[tid]->numOp++;
    thread[tid]->threadStats.numOps++;
    cpuStats.committedOps[tid]++;

    probeInstCommit(inst->staticInst, inst->pcState().instAddr());
}

void
CPU::removeFrontInst(const DynInstPtr &inst)
{
    DPRINTF(O3CPU, "Removing committed instruction [tid:%i] PC %s "
            "[sn:%lli]\n",
            inst->threadNumber, inst->pcState(), inst->seqNum);

    removeInstsThisCycle = true;

    // Remove the front instruction.
    removeList.push(inst->getInstListIt());
}

void
CPU::removeInstsNotInROB(ThreadID tid)
{
    DPRINTF(O3CPU, "Thread %i: Deleting instructions from instruction"
            " list.\n", tid);

    ListIt end_it;

    bool rob_empty = false;

    if (instList.empty()) {
        return;
    } else if (rob.isEmpty(tid)) {
        DPRINTF(O3CPU, "ROB is empty, squashing all insts.\n");
        end_it = instList.begin();
        rob_empty = true;
    } else {
        end_it = (rob.readTailInst(tid))->getInstListIt();
        DPRINTF(O3CPU, "ROB is not empty, squashing insts not in ROB.\n");
    }

    removeInstsThisCycle = true;

    ListIt inst_it = instList.end();

    inst_it--;

    // Walk through the instruction list, removing any instructions
    // that were inserted after the given instruction iterator, end_it.
    while (inst_it != end_it) {
        assert(!instList.empty());

        squashInstIt(inst_it, tid);

        inst_it--;
    }

    // If the ROB was empty, then we actually need to remove the first
    // instruction as well.
    if (rob_empty) {
        squashInstIt(inst_it, tid);
    }
}

void
CPU::removeInstsUntil(const InstSeqNum &seq_num, ThreadID tid)
{
    assert(!instList.empty());

    removeInstsThisCycle = true;

    ListIt inst_iter = instList.end();

    inst_iter--;

    DPRINTF(O3CPU, "Deleting instructions from instruction "
            "list that are from [tid:%i] and above [sn:%lli] (end=%lli).\n",
            tid, seq_num, (*inst_iter)->seqNum);

    while ((*inst_iter)->seqNum > seq_num) {

        bool break_loop = (inst_iter == instList.begin());

        squashInstIt(inst_iter, tid);

        inst_iter--;

        if (break_loop)
            break;
    }
}

void
CPU::squashInstIt(const ListIt &instIt, ThreadID tid)
{
    if ((*instIt)->threadNumber == tid) {
        DPRINTF(O3CPU, "Squashing instruction, "
                "[tid:%i] [sn:%lli] PC %s\n",
                (*instIt)->threadNumber,
                (*instIt)->seqNum,
                (*instIt)->pcState());

        // Mark it as squashed.
        (*instIt)->setSquashed();

        // @todo: Formulate a consistent method for deleting
        // instructions from the instruction list
        // Remove the instruction from the list.
        removeList.push(instIt);
    }
}

void
CPU::cleanUpRemovedInsts()
{
    while (!removeList.empty()) {
        DPRINTF(O3CPU, "Removing instruction, "
                "[tid:%i] [sn:%lli] PC %s\n",
                (*removeList.front())->threadNumber,
                (*removeList.front())->seqNum,
                (*removeList.front())->pcState());

        instList.erase(removeList.front());

        removeList.pop();
    }

    removeInstsThisCycle = false;
}
/*
void
CPU::removeAllInsts()
{
    instList.clear();
}
*/
void
CPU::dumpInsts()
{
    int num = 0;

    ListIt inst_list_it = instList.begin();

    cprintf("Dumping Instruction List\n");

    while (inst_list_it != instList.end()) {
        cprintf("Instruction:%i\nPC:%#x\n[tid:%i]\n[sn:%lli]\nIssued:%i\n"
                "Squashed:%i\n\n",
                num, (*inst_list_it)->pcState().instAddr(),
                (*inst_list_it)->threadNumber,
                (*inst_list_it)->seqNum, (*inst_list_it)->isIssued(),
                (*inst_list_it)->isSquashed());
        inst_list_it++;
        ++num;
    }
}

void
CPU::dumpArchRegs(ThreadID tid)
{
    cprintf("[tid:%i] Dumping architectural registers\n", tid);

    const auto &regClasses = isa[0]->regClasses();
    for (int regTypeIdx = 0; regTypeIdx <= MiscRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);
        if (regType == VecRegClass || regType == VecPredRegClass)
            continue;

        RegClass regClass = regClasses.at(regType);
        size_t numRegs = regClass.numRegs();

        for (RegIndex archIdx = 0; archIdx < numRegs; archIdx++) {
            RegId archReg(regType, archIdx);
            RegVal val;
            if (regType == MiscRegClass) {
                // x86 specific
                if (!TheISA::misc_reg::isValid(archIdx))
                    continue;
                val = readMiscReg(archIdx, tid);
            } else {
                val = getArchReg(archReg, tid);
            }

            cprintf("%s | %s: %s\n",
                    archReg.className(), regClass.regName(archReg),
                    regClass.valString(&val));
        }
    }
}

bool
CPU::canEnterRunahead(ThreadID tid, const DynInstPtr &inst)
{
    if (!runaheadEnabled)
        return false;

    if (inRunahead(tid)) {
        DPRINTF(RunaheadCPU, "[tid:%i] Already in runahead\n", tid);
        return false;
    }

    // Check if this period is potentially too short
    Cycles inFlightCycles = ticksToCycles(curTick() - inst->firstIssue);
    assert(inFlightCycles > Cycles(0));
    if (inFlightCycles > runaheadInFlightThreshold) {
        DPRINTF(RunaheadCPU, "[tid:%i] Cannot enter runahead, load has been in-flight too long.\n", tid);
        cpuStats.refusedRunaheadEntries[cpuStats.ExpectedReturnSoon]++;
        return false;
    }

    // Check that this period won't overlap with a previous one
    // I.e. we must have retired enough insts to catch up with the work runahead did
    if (commit.instsBetweenRunahead[tid] < commit.instsPseudoretired[tid]) {
        DPRINTF(RunaheadCPU, "[tid:%i] Cannot enter runahead, period would overlap.\n", tid);
        cpuStats.refusedRunaheadEntries[cpuStats.OverlappingPeriod]++;
        return false;
    }

    return true;
}

void
CPU::enterRunahead(ThreadID tid)
{
    DynInstPtr robHead = rob.readHeadInst(tid);
    assert(robHead->isLoad() && !robHead->isSquashed() && !robHead->isRunahead());

    if (!canEnterRunahead(tid, robHead))
        return;

    DPRINTF(RunaheadCPU, "[tid:%i] Entering runahead, caused by sn:%llu (PC %s).\n",
                         tid, robHead->seqNum, robHead->pcState());
    Cycles inFlightCycles = ticksToCycles(curTick() - robHead->firstIssue);
    cpuStats.triggerLLLinFlightCycles.sample(inFlightCycles);

    // DEBUG - dump before runahead starts
    //dumpArchRegs(tid);
    // Also debug, save regs in a simple way to make sure they're the same on exit
    saveStateForValidation(tid);
    archStateCheckpoint.fullSave(tid);

    DPRINTF(RunaheadCPU, "[tid:%i] Switching CPU mode to runahead.\n", tid);
    inRunahead(tid, true);
    // Store the instruction that caused entry into runahead
    runaheadCause[tid] = robHead;

    /**
      * Mark all in-flight instructions as runahead.
      * Note that it is not enough to mark all ROB instructions as runahead.
      * Some instructions may be in frontend buffers,
      * and we need to mark the entire instruction window.
      */
    for (DynInstPtr inst : instList) {
        // Committed instructions are not considered in-flight
        if (inst->threadNumber != tid || inst->isCommitted())
            continue;

        DPRINTF(RunaheadCPU, "[tid:%i] Marking instruction [sn:%llu] PC %s as runahead\n",
                tid, inst->seqNum, inst->pcState());
        inst->setRunahead();
    }

    // Invalidate R cache for the upcoming runahead period
    runaheadCache.invalidateCache();
    // Poison the LLL and "execute" it so it can drain out.
    handleRunaheadLLL(robHead);

    commit.instsPseudoretired[tid] = 0;
    runaheadEnteredTick = curTick();
    cpuStats.runaheadPeriods++;
}

void
CPU::runaheadLLLReturn(ThreadID tid)
{
    DPRINTF(RunaheadCPU, "[tid:%i] Signalling commit that runahead is safe to exit.\n", tid);
    const DynInstPtr &lll = runaheadCause[tid];
    commit.signalExitRunahead(tid, lll);
}

void
CPU::exitRunahead(ThreadID tid)
{
    Tick timeInRunahead = ticksToCycles(curTick() - runaheadEnteredTick);
    DPRINTF(RunaheadCPU, "[tid:%i] Exiting runahead after %llu cycles. Instructions pseudoretired: %i\n",
                         tid, timeInRunahead, commit.instsPseudoretired[tid]);

    cpuStats.runaheadCycles.sample(timeInRunahead);
    cpuStats.instsPseudoRetiredPerPeriod.sample(commit.instsPseudoretired[tid]);
    cpuStats.instsFetchedBetweenRunahead.sample(fetch.instsBetweenRunahead[tid]);
    cpuStats.instsRetiredBetweenRunahead.sample(commit.instsBetweenRunahead[tid]);

    // Resume normal mode
    DPRINTF(RunaheadCPU, "[tid:%i] Switching CPU mode to normal.\n", tid);
    inRunahead(tid, false);

    fetch.instsBetweenRunahead[tid] = 0;
    commit.instsBetweenRunahead[tid] = 0;
}

void
CPU::handleRunaheadLLL(const DynInstPtr &inst)
{
    assert(inst->isLoad() && inst->hasRequest());

    // Poison the LLL, mark it as executed
    inst->setPoisoned();
    inst->setExecuted();

    // Have the LSQ forge a response for the LLL
    iew.ldstQueue.forgeResponse(inst);
}

void
CPU::restoreCheckpointState(ThreadID tid)
{
    DPRINTF(RunaheadCPU, "[tid:%i] Restoring architectural state after runahead squash.\n", tid);

    // The ROB should be squashing, empty or fully squashed
    rob.archRestoreSanityCheck(tid);

    // Reset the free list
    freeList.reset();
    // Reset the rename maps
    const BaseISA::RegClasses &regClasses = isa[tid]->regClasses();
    // TODO: this grabs 2 physregs for each arch reg, one for rename and one for commit
    // this essentially nukes a full set of archregs from the phys regfile
    renameMap[tid].reset(regClasses);
    commitRenameMap[tid].reset(regClasses);

    // Clear the rename history buffer to prevent any rename undo shenanigans
    // The history buffer should be empty already, but better safe than sorry!
    rename.clearHistory(tid);

    // Re-initialize the rename maps to be rN -> rN
    // TODO: this assumes 1 active thread. see CPU constructor
    for (int typeIdx = 0; typeIdx <= CCRegClass; typeIdx++) {
        RegClassType regType = static_cast<RegClassType>(typeIdx);
        size_t numRegs = regClasses.at(regType).numRegs();
        for (RegIndex archIdx = 0; archIdx < numRegs; ++archIdx) {
            RegId archReg = RegId(regType, archIdx);
            PhysRegIdPtr physReg = freeList.getReg(regType);

            // Rename maps will agree after runahead exits
            renameMap[tid].setEntry(archReg, physReg);
            commitRenameMap[tid].setEntry(archReg, physReg);

            // Fix the scoreboard while we're at it
            scoreboard.setReg(physReg);
        }
    }

    // Restore architectural registers
    archStateCheckpoint.restore(tid);
    // Clear all register poison
    regFile.clearPoison();
    possiblyDiverging(tid, false);

    // DEBUG - dump arch regs after checkpoint restore
    //dumpArchRegs(tid);
    // Also debug, validate that all checkpoints were successfully restored
    checkStateForValidation(tid);
}

bool
CPU::instCausedRunahead(const DynInstPtr &inst)
{
    ThreadID tid = inst->threadNumber;
    // The thread isn't even running ahead
    if (!inRunahead(tid))
        return false;

    return (inst == runaheadCause[tid]);
}

void
CPU::updateArchCheckpoint(ThreadID tid, const DynInstPtr &inst)
{
    if (!runaheadEnabled)
        return;
    assert(!inRunahead(tid));

    DPRINTF(RunaheadCheckpoint, "[tid:%i] [sn:%llu] Update arch checkpoint to PC %s\n",
                                tid, inst->seqNum, inst->pcState());

    // Update normal regs
    for (int i = 0; i < inst->numDestRegs(); i++) {
        archStateCheckpoint.updateReg(tid, inst->flattenedDestIdx(i));
    }

    // Update any misc regs that were also touched by the instruction
    for (int i = 0; i < inst->numMiscDestRegs(); i++) {
        const RegId miscReg = inst->miscRegIdx(i);
        archStateCheckpoint.updateReg(tid, miscReg);
    }
}

void
CPU::regPoisoned(PhysRegIdPtr physReg, bool poisoned)
{
    if (poisoned) {
        switch (physReg->classValue()) {
        case IntRegClass:
            cpuStats.intRegPoisoned++;
            break;
        case FloatRegClass:
            cpuStats.floatRegPoisoned++;
            break;
        case CCRegClass:
            cpuStats.ccRegPoisoned++;
            break;
        case VecRegClass:
        case VecElemClass:
            cpuStats.vecRegPoisoned++;
            break;
        case VecPredRegClass:
            cpuStats.vecPredRegPoisoned++;
            break;
        case MiscRegClass:
            cpuStats.miscRegPoisoned++;
            break;
        default:
            break;
        }
    } else {
        switch (physReg->classValue()) {
        case IntRegClass:
            cpuStats.intRegCured++;
            break;
        case FloatRegClass:
            cpuStats.floatRegCured++;
            break;
        case CCRegClass:
            cpuStats.ccRegCured++;
            break;
        case VecRegClass:
        case VecElemClass:
            cpuStats.vecRegCured++;
            break;
        case VecPredRegClass:
            cpuStats.vecPredRegCured++;
            break;
        case MiscRegClass:
            cpuStats.miscRegCured++;
            break;
        default:
            break;
        }
    }
    if (poisoned) {
        DPRINTF(RunaheadCPU, "Poisoning physreg %i (flat: %i) (type: %s)\n",
                physReg->index(), physReg->flatIndex(), physReg->className());
    } else {
        DPRINTF(RunaheadCPU, "Curing physreg %i (flat: %i) (type: %s)\n",
                physReg->index(), physReg->flatIndex(), physReg->className());
    }
    regFile.regPoisoned(physReg, poisoned);
}

void
CPU::saveStateForValidation(ThreadID tid)
{
    const auto &regClasses = isa[0]->regClasses();
    for (int regTypeIdx = 0; regTypeIdx <= CCRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);

        // Don't save vecreg and vecpredreg
        if (regType == VecRegClass || regType == VecPredRegClass)
            continue;

        RegClass regClass = regClasses.at(regType);
        size_t numRegs = regClass.numRegs();

        _debugRegVals[regType].clear();
        _debugRegVals[regType].resize(numRegs);

        for (RegIndex archIdx = 0; archIdx < numRegs; archIdx++) {
            RegId archReg(regType, archIdx);

            // Save arch reg values
            RegVal val = getArchReg(archReg, tid);
            _debugRegVals[regType].at(archIdx) = val;
        }
    }

    // Save misc regs
    size_t numMiscRegs = regClasses.at(MiscRegClass).numRegs();
    _debugRegVals[MiscRegClass].clear();
    _debugRegVals[MiscRegClass].resize(numMiscRegs);

    for (RegIndex regIdx = 0; regIdx < numMiscRegs; regIdx++) {
        // x86 specific
        if (!TheISA::misc_reg::isValid(regIdx))
            continue;
        RegVal val = readMiscReg(regIdx, tid);
        _debugRegVals[MiscRegClass].at(regIdx) = val;
    }
}

void
CPU::checkStateForValidation(ThreadID tid)
{
    // Check normal registers and the rename map
    const auto &regClasses = isa[0]->regClasses();
    for (int regTypeIdx = 0; regTypeIdx <= CCRegClass; regTypeIdx++) {
        RegClassType regType = static_cast<RegClassType>(regTypeIdx);

        // Don't save vecreg and vecpredreg
        if (regType == VecRegClass || regType == VecPredRegClass)
            continue;

        RegClass regClass = regClasses.at(regType);
        size_t numRegs = regClass.numRegs();

        for (RegIndex archIdx = 0; archIdx < numRegs; archIdx++) {
            RegId archReg(regType, archIdx);

            RegVal val = getArchReg(archReg, tid);
            RegVal storedVal = _debugRegVals[regType].at(archIdx);
            if (storedVal != val) {
                panic("Stored register mismatch: %s %s - (cur) %s != %s (stored)\n",
                      archReg.className(), regClass.regName(archReg),
                      regClass.valString(&val), regClass.valString(&storedVal));
            }
        }
    }

    // Check misc reg values
    RegClass miscRegClass = regClasses.at(MiscRegClass);
    size_t numMiscRegs = miscRegClass.numRegs();
    for (RegIndex regIdx = 0; regIdx < numMiscRegs; regIdx++) {
        // x86 specific
        if (!TheISA::misc_reg::isValid(regIdx))
            continue;

        RegId archReg(MiscRegClass, regIdx);
        RegVal val = readMiscReg(regIdx, tid);
        RegVal storedVal = _debugRegVals[MiscRegClass].at(regIdx);
        if (storedVal != val) {
            DPRINTF(RunaheadCPU, "Stored misc register mismatch: %s %s - (cur) %s != %s (stored)\n",
                  archReg.className(), miscRegClass.regName(archReg),
                  miscRegClass.valString(&val), miscRegClass.valString(&storedVal));
        }
    }
}

void
CPU::wakeCPU()
{
    if (activityRec.active() || tickEvent.scheduled()) {
        DPRINTF(Activity, "CPU already running.\n");
        return;
    }

    DPRINTF(Activity, "Waking up CPU\n");

    Cycles cycles(curCycle() - lastRunningCycle);
    // @todo: This is an oddity that is only here to match the stats
    if (cycles > 1) {
        --cycles;
        cpuStats.idleCycles += cycles;
        baseStats.numCycles += cycles;
    }

    schedule(tickEvent, clockEdge());
}

void
CPU::wakeup(ThreadID tid)
{
    if (thread[tid]->status() != gem5::ThreadContext::Suspended)
        return;

    wakeCPU();

    DPRINTF(Quiesce, "Suspended Processor woken\n");
    threadContexts[tid]->activate();
}

ThreadID
CPU::getFreeTid()
{
    for (ThreadID tid = 0; tid < numThreads; tid++) {
        if (!tids[tid]) {
            tids[tid] = true;
            return tid;
        }
    }

    return InvalidThreadID;
}

void
CPU::updateThreadPriority()
{
    if (activeThreads.size() > 1) {
        //DEFAULT TO ROUND ROBIN SCHEME
        //e.g. Move highest priority to end of thread list
        std::list<ThreadID>::iterator list_begin = activeThreads.begin();

        unsigned high_thread = *list_begin;

        activeThreads.erase(list_begin);

        activeThreads.push_back(high_thread);
    }
}

void
CPU::addThreadToExitingList(ThreadID tid)
{
    DPRINTF(O3CPU, "Thread %d is inserted to exitingThreads list\n", tid);

    // the thread trying to exit can't be already halted
    assert(tcBase(tid)->status() != gem5::ThreadContext::Halted);

    // make sure the thread has not been added to the list yet
    assert(exitingThreads.count(tid) == 0);

    // add the thread to exitingThreads list to mark that this thread is
    // trying to exit. The boolean value in the pair denotes if a thread is
    // ready to exit. The thread is not ready to exit until the corresponding
    // exit trap event is processed in the future. Until then, it'll be still
    // an active thread that is trying to exit.
    exitingThreads.emplace(std::make_pair(tid, false));
}

bool
CPU::isThreadExiting(ThreadID tid) const
{
    return exitingThreads.count(tid) == 1;
}

void
CPU::scheduleThreadExitEvent(ThreadID tid)
{
    assert(exitingThreads.count(tid) == 1);

    // exit trap event has been processed. Now, the thread is ready to exit
    // and be removed from the CPU.
    exitingThreads[tid] = true;

    // we schedule a threadExitEvent in the next cycle to properly clean
    // up the thread's states in the pipeline. threadExitEvent has lower
    // priority than tickEvent, so the cleanup will happen at the very end
    // of the next cycle after all pipeline stages complete their operations.
    // We want all stages to complete squashing instructions before doing
    // the cleanup.
    if (!threadExitEvent.scheduled()) {
        schedule(threadExitEvent, nextCycle());
    }
}

void
CPU::exitThreads()
{
    // there must be at least one thread trying to exit
    assert(exitingThreads.size() > 0);

    // terminate all threads that are ready to exit
    auto it = exitingThreads.begin();
    while (it != exitingThreads.end()) {
        ThreadID thread_id = it->first;
        bool readyToExit = it->second;

        if (readyToExit) {
            DPRINTF(O3CPU, "Exiting thread %d\n", thread_id);
            haltContext(thread_id);
            tcBase(thread_id)->setStatus(gem5::ThreadContext::Halted);
            it = exitingThreads.erase(it);
        } else {
            it++;
        }
    }
}

void
CPU::htmSendAbortSignal(ThreadID tid, uint64_t htm_uid,
        HtmFailureFaultCause cause)
{
    const Addr addr = 0x0ul;
    const int size = 8;
    const Request::Flags flags =
      Request::PHYSICAL|Request::STRICT_ORDER|Request::HTM_ABORT;

    // Runahead-specific actions
    iew.ldstQueue.resetHtmStartsStops(tid);
    commit.resetHtmStartsStops(tid);

    // notify l1 d-cache (ruby) that core has aborted transaction
    RequestPtr req =
        std::make_shared<Request>(addr, size, flags, _dataRequestorId);

    req->taskId(taskId());
    req->setContext(thread[tid]->contextId());
    req->setHtmAbortCause(cause);

    assert(req->isHTMAbort());

    PacketPtr abort_pkt = Packet::createRead(req);
    uint8_t *memData = new uint8_t[8];
    assert(memData);
    abort_pkt->dataStatic(memData);
    abort_pkt->setHtmTransactional(htm_uid);

    // TODO include correct error handling here
    if (!iew.ldstQueue.getDataPort().sendTimingReq(abort_pkt)) {
        panic("HTM abort signal was not sent to the memory subsystem.");
    }
}

} // namespace runahead
} // namespace gem5
