from typing import Any, Union
from gem5.isas import ISA
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_switchable_processor import SimpleSwitchableProcessor
from gem5.components.processors.simple_processor import SimpleProcessor
from m5.objects.BranchPredictor import TAGE_SC_L_8KB
from gem5.components.processors.simple_core import SimpleCore
from m5.objects import SimObject
from m5.objects.SimPoint import SimPoint
from simpoints import parse_simpoints

def add_core_args(parser):
    cpu_group = parser.add_argument_group(title='CPU Parameters')

    cpu_group.add_argument('--no-runahead', action='store_false', dest='enable_runahead')
    cpu_group.add_argument('--lll-threshold', default=3, help='Memory depth at which a load is considered a LLL')
    cpu_group.add_argument('--rcache-size', default='2kB', help='Size of the runahead cache')
    cpu_group.add_argument('--runahead-exit-policy', default='Eager', help='Runahead exit policy')
    cpu_group.add_argument('--fixed-exit-latency', default=100, help='If using FixedDelayed RE exit policy, how long to wait before exiting')
    cpu_group.add_argument('--lll-latency-threshold', default=100, help='Max load latency before runahead can no longer be entered')
    cpu_group.add_argument('--overlapping-runahead', action='store_true', dest='overlapping_runahead', help='Allow overlapping runahead periods')
    cpu_group.set_defaults(enable_runahead=True, overlapping_runahead=False)

    cpu_group.add_argument('--rob-size', default=224, type=int, help='The amount of ROB entries')

    cpu_group.add_argument('--fetch-width', default=4, type=int, help='Fetch stage width')
    cpu_group.add_argument('--decode-width', default=4, type=int, help='Decode stage width')
    cpu_group.add_argument('--rename-width', default=4, type=int, help='Rename stage width')
    cpu_group.add_argument('--issue-width', default=4, type=int, help='Issue stage width')
    cpu_group.add_argument('--writeback-width', default=8, type=int, help='WB stage width')
    cpu_group.add_argument('--commit-width', default=8, type=int, help='Commit stage width')

    cpu_group.add_argument('--iq-size', default=97, type=int, help='Issue queue entries')
    cpu_group.add_argument('--lq-size', default=64, type=int, help='Load queue entries')
    cpu_group.add_argument('--sq-size', default=60, type=int, help='Store queue entries')

    cpu_group.add_argument('--int-regs', default=180, type=int, help='Integer registers')
    cpu_group.add_argument('--fp-regs', default=180, type=int, help='FP registers')
    cpu_group.add_argument('--vec-regs', default=96, type=int, help='Vector registers')

    cpu_group.add_argument('--int-alus', default=3, type=int, help='Integer ALUs')
    cpu_group.add_argument('--int-mds', default=1, type=int, help='Integer multiply/divide FUs')
    cpu_group.add_argument('--fp-alus', default=1, type=int, help='Floating point ALUs')
    cpu_group.add_argument('--fp-mds', default=1, type=int, help='Floating point multiply/divide FUs')
    cpu_group.add_argument('--mem-ports', default=2, type=int, help='Memory port FUs')

def setup_simpoint_processor(args) -> SimpleProcessor:
    print('Creating atomic processor for simpoint profiling')
    processor = SimpleProcessor(cpu_type=CPUTypes.ATOMIC, num_cores=1, isa=ISA.X86)
    sim_core = processor.get_cores()[0].core

    if args.max_insts > 0:
        sim_core.max_insts_any_thread = args.max_insts

    # Setup the simpoint probe if doing simpoint profiling
    if args.simpoint_interval > 0 and not args.simpoint_checkpoints:
        simpoint = SimPoint()
        simpoint.interval = args.simpoint_interval
        # Needed to make gem5 instantiate and register the simpoint with the core
        sim_core.simpointProbeListener = simpoint

    return processor

def setup_runahead_processor(args) -> SimpleSwitchableProcessor:
    print('Creating switchable processor (atomic -> runahead)')
    switch_processor = SimpleSwitchableProcessor(
        starting_core_type=CPUTypes.ATOMIC,
        switch_core_type=CPUTypes.RUNAHEAD,
        num_cores=1,
        isa=ISA.X86,
    )

    atomic_cores = list(filter(
        lambda c: c[0].get_type() == CPUTypes.ATOMIC,
        switch_processor._switchable_cores.values()
    ))[0]

    core: SimpleCore
    for core in atomic_cores:
        sim_core: SimObject = core.core
        print(f'Configuring {sim_core}...')

        # If we're taking simpoint checkpoints, setup the start counts for the highest weight simpoint
        if args.simpoint_checkpoints:
            simpoints = parse_simpoints(args, highest_weight_only=True)
            start_insts = []
            for sp in simpoints:
                start_inst = (sp['insts'] - sp['warmup'])
                weight = sp["weight"]*100
                print(f'Inserting simpoint #{sp["id"]} (W={weight:.2f}%): at {sp["insts"]} insts, {sp["warmup"]} warmup insts => start at {start_inst} insts.')
                start_insts.append(start_inst)
            sim_core.simpoint_start_insts = start_insts

    runahead_cores = list(filter(
        lambda c: c[0].get_type() == CPUTypes.RUNAHEAD,
        switch_processor._switchable_cores.values()
    ))[0]

    core: SimpleCore
    for core in runahead_cores:
        # grab the simobject core. yeah it doesn't make much sense.
        sim_core: SimObject = core.core
        print(f'Configuring {sim_core}...')

        # Max insts to simulate
        if args.max_insts > 0:
            sim_core.max_insts_any_thread = args.max_insts

        # Setup runahead parameters
        sim_core.enableRunahead = args.enable_runahead
        sim_core.lllDepthThreshold = args.lll_threshold
        sim_core.runaheadCacheSize = args.rcache_size
        sim_core.runaheadExitPolicy = args.runahead_exit_policy
        sim_core.runaheadFixedExitLength = args.fixed_exit_latency
        sim_core.runaheadInFlightThreshold = args.lll_latency_threshold
        sim_core.allowOverlappingRunahead = args.overlapping_runahead

        # setup O3 core parameters
        sim_core.fetchWidth = args.fetch_width
        sim_core.decodeWidth = args.decode_width
        sim_core.renameWidth = args.rename_width
        sim_core.issueWidth = args.issue_width
        sim_core.wbWidth = args.writeback_width
        sim_core.commitWidth = args.commit_width

        sim_core.numROBEntries = args.rob_size

        sim_core.numIQEntries = args.iq_size
        sim_core.LQEntries = args.lq_size
        sim_core.SQEntries = args.sq_size

        sim_core.numPhysIntRegs = args.int_regs
        sim_core.numPhysFloatRegs = args.fp_regs
        sim_core.numPhysVecRegs = args.vec_regs

        sim_core.branchPred = TAGE_SC_L_8KB()

        # Functional units
        sim_core.fuPool.FUList[0].count = args.int_alus  # int ALU
        sim_core.fuPool.FUList[1].count = args.int_mds  # int mul/div
        sim_core.fuPool.FUList[2].count = args.fp_alus  # fp ALU
        sim_core.fuPool.FUList[3].count = args.fp_mds  # fp mul/div
        sim_core.fuPool.FUList[8].count = args.mem_ports  # r/w mem port

    return switch_processor


def setup_cores(args) -> Union[SimpleProcessor, SimpleSwitchableProcessor]:
    print('Configuring processor...')
    # If taking checkpoints we MUST use the detailed system configuration
    # because checkpoints are not portable across system configurations!
    if args.simpoint_interval > 0 and not args.simpoint_checkpoints:
        return setup_simpoint_processor(args)
    else:
        return setup_runahead_processor(args)
