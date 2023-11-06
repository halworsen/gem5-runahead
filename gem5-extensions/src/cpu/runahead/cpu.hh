/*
 * Copyright (c) 2011-2013, 2016-2020 ARM Limited
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
 * Copyright (c) 2004-2005 The Regents of The University of Michigan
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

#ifndef __CPU_RUNAHEAD_CPU_HH__
#define __CPU_RUNAHEAD_CPU_HH__

#include <iostream>
#include <list>
#include <queue>
#include <set>
#include <vector>

#include "arch/generic/pcstate.hh"
#include "base/statistics.hh"
#include "config/the_isa.hh"
#include "cpu/runahead/arch_checkpoint.hh"
#include "cpu/runahead/comm.hh"
#include "cpu/runahead/commit.hh"
#include "cpu/runahead/decode.hh"
#include "cpu/runahead/dyn_inst_ptr.hh"
#include "cpu/runahead/fetch.hh"
#include "cpu/runahead/free_list.hh"
#include "cpu/runahead/iew.hh"
#include "cpu/runahead/limits.hh"
#include "cpu/runahead/rename.hh"
#include "cpu/runahead/rob.hh"
#include "cpu/runahead/runahead_cache.hh"
#include "cpu/runahead/scoreboard.hh"
#include "cpu/runahead/thread_state.hh"
#include "cpu/activity.hh"
#include "cpu/base.hh"
#include "cpu/simple_thread.hh"
#include "cpu/timebuf.hh"
#include "params/BaseRunaheadCPU.hh"
#include "sim/process.hh"

namespace gem5
{

template <class>
class Checker;
class ThreadContext;

class Checkpoint;
class Process;

namespace runahead
{

class ThreadContext;

/**
 * RunaheadCPU class, has each of the stages (fetch through commit)
 * within it, as well as all of the time buffers between stages.  The
 * tick() function for the CPU is defined here.
 */
class CPU : public BaseCPU
{
  public:
    typedef std::list<DynInstPtr>::iterator ListIt;

    friend class ThreadContext;
    friend class ArchCheckpoint;

  public:
    enum Status
    {
        Running,
        Idle,
        Halted,
        Blocked,
        SwitchedOut
    };

    BaseMMU *mmu;
    using LSQRequest = LSQ::LSQRequest;

    /** Overall CPU status. */
    Status _status;

  private:

    /** The tick event used for scheduling CPU ticks. */
    EventFunctionWrapper tickEvent;

    /** The exit event used for terminating all ready-to-exit threads */
    EventFunctionWrapper threadExitEvent;

    /** Schedule tick event, regardless of its current state. */
    void
    scheduleTickEvent(Cycles delay)
    {
        if (tickEvent.squashed())
            reschedule(tickEvent, clockEdge(delay));
        else if (!tickEvent.scheduled())
            schedule(tickEvent, clockEdge(delay));
    }

    /** Unschedule tick event, regardless of its current state. */
    void
    unscheduleTickEvent()
    {
        if (tickEvent.scheduled())
            tickEvent.squash();
    }

    /**
     * Check if the pipeline has drained and signal drain done.
     *
     * This method checks if a drain has been requested and if the CPU
     * has drained successfully (i.e., there are no instructions in
     * the pipeline). If the CPU has drained, it deschedules the tick
     * event and signals the drain manager.
     *
     * @return False if a drain hasn't been requested or the CPU
     * hasn't drained, true otherwise.
     */
    bool tryDrain();

    /**
     * Perform sanity checks after a drain.
     *
     * This method is called from drain() when it has determined that
     * the CPU is fully drained when gem5 is compiled with the NDEBUG
     * macro undefined. The intention of this method is to do more
     * extensive tests than the isDrained() method to weed out any
     * draining bugs.
     */
    void drainSanityCheck() const;

    /** Check if a system is in a drained state. */
    bool isCpuDrained() const;

  public:
    /** Constructs a CPU with the given parameters. */
    CPU(const BaseRunaheadCPUParams &params);

    ProbePointArg<PacketPtr> *ppInstAccessComplete;
    ProbePointArg<std::pair<DynInstPtr, PacketPtr> > *ppDataAccessComplete;

    /** Register probe points. */
    void regProbePoints() override;

    void
    demapPage(Addr vaddr, uint64_t asn)
    {
        mmu->demapPage(vaddr, asn);
    }

    /** Ticks CPU, calling tick() on each stage, and checking the overall
     *  activity to see if the CPU should deschedule itself.
     */
    void tick();

    /** Initialize the CPU */
    void init() override;

    void startup() override;

    /** Returns the Number of Active Threads in the CPU */
    int
    numActiveThreads()
    {
        return activeThreads.size();
    }

    /** Add Thread to Active Threads List */
    void activateThread(ThreadID tid);

    /** Remove Thread from Active Threads List */
    void deactivateThread(ThreadID tid);

    /** Setup CPU to insert a thread's context */
    void insertThread(ThreadID tid);

    /** Remove all of a thread's context from CPU */
    void removeThread(ThreadID tid);

    /** Count the Total Instructions Committed in the CPU. */
    Counter totalInsts() const override;

    /** Count the Total Ops (including micro ops) committed in the CPU. */
    Counter totalOps() const override;

    /** Add Thread to Active Threads List. */
    void activateContext(ThreadID tid) override;

    /** Remove Thread from Active Threads List */
    void suspendContext(ThreadID tid) override;

    /** Remove Thread from Active Threads List &&
     *  Remove Thread Context from CPU.
     */
    void haltContext(ThreadID tid) override;

    /** Update The Order In Which We Process Threads. */
    void updateThreadPriority();

    /** Is the CPU draining? */
    bool isDraining() const { return drainState() == DrainState::Draining; }

    void serializeThread(CheckpointOut &cp, ThreadID tid) const override;
    void unserializeThread(CheckpointIn &cp, ThreadID tid) override;

    /** Insert tid to the list of threads trying to exit */
    void addThreadToExitingList(ThreadID tid);

    /** Is the thread trying to exit? */
    bool isThreadExiting(ThreadID tid) const;

    /**
     *  If a thread is trying to exit and its corresponding trap event
     *  has been completed, schedule an event to terminate the thread.
     */
    void scheduleThreadExitEvent(ThreadID tid);

    /** Terminate all threads that are ready to exit */
    void exitThreads();

  public:
    /** Starts draining the CPU's pipeline of all instructions in
     * order to stop all memory accesses. */
    DrainState drain() override;

    /** Resumes execution after a drain. */
    void drainResume() override;

    /**
     * Commit has reached a safe point to drain a thread.
     *
     * Commit calls this method to inform the pipeline that it has
     * reached a point where it is not executed microcode and is about
     * to squash uncommitted instructions to fully drain the pipeline.
     */
    void commitDrained(ThreadID tid);

    /** Switches out this CPU. */
    void switchOut() override;

    /** Takes over from another CPU. */
    void takeOverFrom(BaseCPU *oldCPU) override;

    void verifyMemoryMode() const override;

    /** Get the current instruction sequence number, and increment it. */
    InstSeqNum getAndIncrementInstSeq() { return globalSeqNum++; }

    /** Traps to handle given fault. */
    void trap(const Fault &fault, ThreadID tid, const StaticInstPtr &inst);

    /** Returns the Fault for any valid interrupt. */
    Fault getInterrupts();

    /** Processes any an interrupt fault. */
    void processInterrupts(const Fault &interrupt);

    /** Halts the CPU. */
    void halt() { panic("Halt not implemented!\n"); }

    /** Register accessors.  Index refers to the physical register index. */

    /** Reads a miscellaneous register. */
    RegVal readMiscRegNoEffect(int misc_reg, ThreadID tid) const;

    /** Reads a misc. register, including any side effects the read
     * might have as defined by the architecture.
     */
    RegVal readMiscReg(int misc_reg, ThreadID tid);

    /** Sets a miscellaneous register. */
    void setMiscRegNoEffect(int misc_reg, RegVal val, ThreadID tid);

    /** Sets a misc. register, including any side effects the write
     * might have as defined by the architecture.
     */
    void setMiscReg(int misc_reg, RegVal val, ThreadID tid);

    RegVal getReg(PhysRegIdPtr phys_reg);
    void getReg(PhysRegIdPtr phys_reg, void *val);
    void *getWritableReg(PhysRegIdPtr phys_reg);

    void setReg(PhysRegIdPtr phys_reg, RegVal val);
    void setReg(PhysRegIdPtr phys_reg, const void *val);

    /** Architectural register accessors.  Looks up in the commit
     * rename table to obtain the true physical index of the
     * architected register first, then accesses that physical
     * register.
     */

    RegVal getArchReg(const RegId &reg, ThreadID tid);
    void getArchReg(const RegId &reg, void *val, ThreadID tid);
    void *getWritableArchReg(const RegId &reg, ThreadID tid);

    void setArchReg(const RegId &reg, RegVal val, ThreadID tid);
    void setArchReg(const RegId &reg, const void *val, ThreadID tid);

    /** Sets the commit PC state of a specific thread. */
    void pcState(const PCStateBase &new_pc_state, ThreadID tid);

    /** Reads the commit PC state of a specific thread. */
    const PCStateBase &pcState(ThreadID tid);

    /** Initiates a squash of all in-flight instructions for a given
     * thread.  The source of the squash is an external update of
     * state through the TC.
     */
    void squashFromTC(ThreadID tid);

    /** Function to add instruction onto the head of the list of the
     *  instructions.  Used when new instructions are fetched.
     */
    ListIt addInst(const DynInstPtr &inst);

    /** Function to tell the CPU that an instruction has completed. */
    void instDone(ThreadID tid, const DynInstPtr &inst);

    /** Remove an instruction from the front end of the list.  There's
     *  no restriction on location of the instruction.
     */
    void removeFrontInst(const DynInstPtr &inst);

    /** Remove all instructions that are not currently in the ROB.
     *  There's also an option to not squash delay slot instructions.*/
    void removeInstsNotInROB(ThreadID tid);

    /** Remove all instructions younger than the given sequence number. */
    void removeInstsUntil(const InstSeqNum &seq_num, ThreadID tid);

    /** Removes the instruction pointed to by the iterator. */
    void squashInstIt(const ListIt &instIt, ThreadID tid);

    /** Cleans up all instructions on the remove list. */
    void cleanUpRemovedInsts();

    /** Debug function to print all instructions on the list. */
    void dumpInsts();

    /** Debug function to print all architectural registers */
    void dumpArchRegs(ThreadID tid);

private:
    /** Whether or not runahead is enabled */
    bool runaheadEnabled;

    /** The in-flight threshold for runahead entry */
    Cycles runaheadInFlightThreshold;

    /** Allow entry to runahead if it would overlap periods? */
    bool allowOverlappingRunahead;

    /** Tracks which threads are in runahead */
    bool runaheadStatus[MaxThreads] = { false };

    /**
     * True if the CPU is not currently processing a cycle (i.e. the CPU is between ticks)
     * Mostly for debugging purposes (e.g. setting breakpoints on events that happen off-tick)
    */
    bool offTick = true;

    /** Whether or not the CPU is possibly diverging from correct execution */
    bool branchDivergence[MaxThreads] = { false };

    /** Debug for saving and validating checkpointed state */

    /** Architectural register values */
    std::array<std::vector<RegVal>, MiscRegClass + 1> _debugRegVals;

    /** Save any state */
    void saveStateForValidation(ThreadID tid);

    /** Check that the current CPU state matches up with the save state */
    void checkStateForValidation(ThreadID tid);

public:
    /** The instruction that caused us to enter runahead mode */
    std::array<DynInstPtr, MaxThreads> runaheadCause;

    /** Check if we can enter runahead right now, caused by the given inst */
    bool canEnterRunahead(ThreadID tid, const DynInstPtr &inst);

    /** Enter runahead, starting from the instruction at the head of the ROB */
    void enterRunahead(ThreadID tid);

    /**
     * Signal commit that the runahead-causing LLL has returned
     * Commit will handle the exit on the first coming cycle,
     * which may be the same or the next cycle, depending on exactly when the load returns
     */
    void runaheadLLLReturn(ThreadID tid);

    /** Exit runahead, resuming from the instruction that caused us to enter runahead */
    void exitRunahead(ThreadID tid);

    /** Unblock a long latency load at the head of the ROB */
    void handleRunaheadLLL(const DynInstPtr &inst);

    /** Restore the CPU's architectural state to the last checkpoint */
    void restoreCheckpointState(ThreadID tid);

    /** Find whether or not a thread is currently in runahead */
    bool inRunahead(ThreadID tid) { return runaheadStatus[tid]; };

    /** Set whether or not a thread is in runahead */
    void inRunahead(ThreadID tid, bool state) { runaheadStatus[tid] = state; };

    /** Whether or not the given thread is possibly diverging from correct execution */
    bool possiblyDiverging(ThreadID tid) { return branchDivergence[tid]; };

    /** Set whether or not the given thread is possibly diverging from correct execution */
    void possiblyDiverging(ThreadID tid, bool diverging) { branchDivergence[tid] = diverging; };

    /** Check if the given instruction caused the CPU to enter runahead */
    bool instCausedRunahead(const DynInstPtr &inst);

    /** Update the architectural state checkpoint to reflect CPU state after retirement of inst */
    void updateArchCheckpoint(ThreadID tid, const DynInstPtr &inst);

    /** Check if a register is marked as poisoned/invalid */
    bool regPoisoned(PhysRegIdPtr reg) { return regFile.regPoisoned(reg); };

    /** Mark/unmark a register as poisoned */
    void regPoisoned(PhysRegIdPtr reg, bool poisoned);

    /** The tick at which runahead was last entered */
    Tick runaheadEnteredTick;

    /** The depth at which a blocking memory request is considered a long latency load */
    uint8_t lllDepthThreshold;

    /** Whether or not to enter runahead immediately on seeing a LLL at the ROB head */
    bool runaheadEagerEntry;

  public:
#ifndef NDEBUG
    /** Count of total number of dynamic instructions in flight. */
    int instcount;
#endif

    /** List of all the instructions in flight. */
    std::list<DynInstPtr> instList;

    /** List of all the instructions that will be removed at the end of this
     *  cycle.
     */
    std::queue<ListIt> removeList;

#ifdef DEBUG
    /** Debug structure to keep track of the sequence numbers still in
     * flight.
     */
    std::set<InstSeqNum> snList;
#endif

    /** Records if instructions need to be removed this cycle due to
     *  being retired or squashed.
     */
    bool removeInstsThisCycle;

  protected:
    /** The fetch stage. */
    Fetch fetch;

    /** The decode stage. */
    Decode decode;

    /** The dispatch stage. */
    Rename rename;

    /** The issue/execute/writeback stages. */
    IEW iew;

    /** The commit stage. */
    Commit commit;

    /** The register file. */
    PhysRegFile regFile;

    /** The free list. */
    UnifiedFreeList freeList;

    /** The frontend rename map. */
    UnifiedRenameMap renameMap[MaxThreads];

    /** The commit rename map. */
    UnifiedRenameMap commitRenameMap[MaxThreads];

    /** The re-order buffer. */
    ROB rob;

    /** Runahead cache for holding store writebacks in runahead execution */
    RunaheadCache runaheadCache;

    /** Active Threads List */
    std::list<ThreadID> activeThreads;

    /**
     *  This is a list of threads that are trying to exit. Each thread id
     *  is mapped to a boolean value denoting whether the thread is ready
     *  to exit.
     */
    std::unordered_map<ThreadID, bool> exitingThreads;

    /** Integer Register Scoreboard */
    Scoreboard scoreboard;

    std::vector<TheISA::ISA *> isa;

  private:
    /** Running architectural state checkpoint */
    ArchCheckpoint archStateCheckpoint;

  public:
    /** Enum to give each stage a specific index, so when calling
     *  activateStage() or deactivateStage(), they can specify which stage
     *  is being activated/deactivated.
     */
    enum StageIdx
    {
        FetchIdx,
        DecodeIdx,
        RenameIdx,
        IEWIdx,
        CommitIdx,
        NumStages
    };

    /** The main time buffer to do backwards communication. */
    TimeBuffer<TimeStruct> timeBuffer;

    /** The fetch stage's instruction queue. */
    TimeBuffer<FetchStruct> fetchQueue;

    /** The decode stage's instruction queue. */
    TimeBuffer<DecodeStruct> decodeQueue;

    /** The rename stage's instruction queue. */
    TimeBuffer<RenameStruct> renameQueue;

    /** The IEW stage's instruction queue. */
    TimeBuffer<IEWStruct> iewQueue;

  private:
    /** The activity recorder; used to tell if the CPU has any
     * activity remaining or if it can go to idle and deschedule
     * itself.
     */
    ActivityRecorder activityRec;

  public:
    /** Records that there was time buffer activity this cycle. */
    void activityThisCycle() { activityRec.activity(); }

    /** Changes a stage's status to active within the activity recorder. */
    void
    activateStage(const StageIdx idx)
    {
        activityRec.activateStage(idx);
    }

    /** Changes a stage's status to inactive within the activity recorder. */
    void
    deactivateStage(const StageIdx idx)
    {
        activityRec.deactivateStage(idx);
    }

    /** Wakes the CPU, rescheduling the CPU if it's not already active. */
    void wakeCPU();

    virtual void wakeup(ThreadID tid) override;

    /** Gets a free thread id. Use if thread ids change across system. */
    ThreadID getFreeTid();

  public:
    /** Returns a pointer to a thread context. */
    gem5::ThreadContext *
    tcBase(ThreadID tid)
    {
        return thread[tid]->getTC();
    }

    /** The global sequence number counter. */
    InstSeqNum globalSeqNum;//[MaxThreads];

    /** Pointer to the checker, which can dynamically verify
     * instruction results at run time.  This can be set to NULL if it
     * is not being used.
     */
    gem5::Checker<DynInstPtr> *checker;

    /** Pointer to the system. */
    System *system;

    /** Pointers to all of the threads in the CPU. */
    std::vector<ThreadState *> thread;

    /** Threads Scheduled to Enter CPU */
    std::list<int> cpuWaitList;

    /** The cycle that the CPU was last running, used for statistics. */
    Cycles lastRunningCycle;

    /** The cycle that the CPU was last activated by a new thread*/
    Tick lastActivatedCycle;

    /** Mapping for system thread id to cpu id */
    std::map<ThreadID, unsigned> threadMap;

    /** Available thread ids in the cpu*/
    std::vector<ThreadID> tids;

    /** CPU pushRequest function, forwards request to LSQ. */
    Fault
    pushRequest(const DynInstPtr& inst, bool isLoad, uint8_t *data,
                unsigned int size, Addr addr, Request::Flags flags,
                uint64_t *res, AtomicOpFunctorPtr amo_op = nullptr,
                const std::vector<bool>& byte_enable=std::vector<bool>())

    {
        return iew.ldstQueue.pushRequest(inst, isLoad, data, size, addr,
                flags, res, std::move(amo_op), byte_enable);
    }

    /** Used by the fetch unit to get a hold of the instruction port. */
    Port &
    getInstPort() override
    {
        return fetch.getInstPort();
    }

    /** Get the dcache port (used to find block size for translations). */
    Port &
    getDataPort() override
    {
        return iew.ldstQueue.getDataPort();
    }

    struct CPUStats : public statistics::Group
    {
        CPUStats(CPU *cpu);

        /** Number of cycles spent in runahead */
        statistics::Scalar runaheadCycles;
        /** Number of cycles spent outside of runahead */
        statistics::Scalar realCycles;

        /** Stat for total number of times the CPU is descheduled. */
        statistics::Scalar timesIdled;
        /** Stat for total number of cycles the CPU spends descheduled. */
        statistics::Scalar idleCycles;
        /** Stat for total number of cycles the CPU spends descheduled due to a
         * quiesce operation or waiting for an interrupt. */
        statistics::Scalar quiesceCycles;
        /** Stat for the number of committed instructions per thread. */
        statistics::Vector committedInsts;
        /** Stat for the number of pseudoretired instructions per thread. */
        statistics::Vector pseudoRetiredInsts;
        /** Stat for the number of committed ops (including micro ops) per
         *  thread. */
        statistics::Vector committedOps;
        /** Stat for the CPI per thread. */
        statistics::Formula cpi;
        /** Stat for the total CPI. */
        statistics::Formula totalCpi;
        /** Stat for the IPC per thread. */
        statistics::Formula ipc;
        /** Stat for the total IPC. */
        statistics::Formula totalIpc;
        /** CPI per thread in runahead */
        statistics::Formula runaheadCpi;
        /** IPC per thread in runahead */
        statistics::Formula runaheadIpc;
        /** CPI per thread outside of runahead */
        statistics::Formula realCpi;
        /** IPC per thread outside of runahead */
        statistics::Formula realIpc;

        //number of integer register file accesses
        statistics::Scalar intRegfileReads;
        statistics::Scalar intRegfileWrites;
        //number of float register file accesses
        statistics::Scalar fpRegfileReads;
        statistics::Scalar fpRegfileWrites;
        //number of vector register file accesses
        mutable statistics::Scalar vecRegfileReads;
        statistics::Scalar vecRegfileWrites;
        //number of predicate register file accesses
        mutable statistics::Scalar vecPredRegfileReads;
        statistics::Scalar vecPredRegfileWrites;
        //number of CC register file accesses
        statistics::Scalar ccRegfileReads;
        statistics::Scalar ccRegfileWrites;
        //number of misc
        statistics::Scalar miscRegfileReads;
        statistics::Scalar miscRegfileWrites;

        /** Runahead statistics */

        // Amount of times runahead was entered
        statistics::Scalar runaheadPeriods;
        // Distribution of amount of cycles spent in runahead periods
        statistics::Distribution runaheadCycleDist;
        // Amount of times the CPU refused to enter into runahead
        statistics::Vector refusedRunaheadEntries;
        // Histogram of amount of instructions pseudoretired by runahead execution
        statistics::Histogram instsPseudoRetiredPerPeriod;
        // Histogram of instructions fetched between runahead periods
        statistics::Distribution instsFetchedBetweenRunahead;
        // Histogram of instructions retired between runahead periods
        statistics::Distribution instsRetiredBetweenRunahead;
        // Histogram of cycles a load has been in-flight when it triggered runahead
        statistics::Histogram triggerLLLinFlightCycles;

        // Amount of times an integer register was marked as poisoned
        statistics::Scalar intRegPoisoned;
        // Amount of times an integer register's poison was reset
        statistics::Scalar intRegCured;
        // Amount of times a float register was marked as poisoned
        statistics::Scalar floatRegPoisoned;
        // Amount of times a float register's poison was reset
        statistics::Scalar floatRegCured;
        // Amount of times a vector register was marked as poisoned
        statistics::Scalar vecRegPoisoned;
        // Amount of times a vector register's poison was reset
        statistics::Scalar vecRegCured;
        // Amount of times a predicate register was marked as poisoned
        statistics::Scalar vecPredRegPoisoned;
        // Amount of times a predicate register's poison was reset
        statistics::Scalar vecPredRegCured;
        // Amount of times a CC register was marked as poisoned
        statistics::Scalar ccRegPoisoned;
        // Amount of times a CC register's poison was reset
        statistics::Scalar ccRegCured;
        // Amount of times a misc register was marked as poisoned
        statistics::Scalar miscRegPoisoned;
        // Amount of times a misc register's poison was reset
        statistics::Scalar miscRegCured;

        enum {
            NotStalling,
            ExpectedReturnSoon,
            OverlappingPeriod
        };

    } cpuStats;

  public:
    // hardware transactional memory
    void htmSendAbortSignal(ThreadID tid, uint64_t htm_uid,
                            HtmFailureFaultCause cause) override;
};

} // namespace runahead
} // namespace gem5

#endif // __CPU_RUNAHEAD_CPU_HH__
