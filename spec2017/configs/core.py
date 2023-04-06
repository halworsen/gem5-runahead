from gem5.isas import ISA
from gem5.components.processors.cpu_types import CPUTypes
from gem5.components.processors.simple_core import SimpleCore
from gem5.components.processors.simple_processor import SimpleProcessor
from m5.objects.FUPool import DefaultFUPool
from m5.objects.BranchPredictor import TAGE_SC_L_8KB

def add_core_args(parser):
    cpu_group = parser.add_argument_group(title='O3 CPU Parameters')

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


def setup_cores(args) -> SimpleProcessor:
    o3_processor = SimpleProcessor(cpu_type=CPUTypes.O3, num_cores=1, isa=ISA.X86)

    core = o3_processor.get_cores()[0].core
    # Max instructions to simulate
    # This is set after boot/before the ROI
    # core.max_insts_any_thread = args.max_insts

    # setup O3 core parameters
    core.fetchWidth = args.fetch_width
    core.decodeWidth = args.decode_width
    core.renameWidth = args.rename_width
    core.issueWidth = args.issue_width
    core.wbWidth = args.writeback_width
    core.commitWidth = args.commit_width

    core.numROBEntries = args.rob_size

    core.numIQEntries = args.iq_size
    core.LQEntries = args.lq_size
    core.SQEntries = args.sq_size

    core.numPhysIntRegs = args.int_regs
    core.numPhysFloatRegs = args.fp_regs
    core.numPhysVecRegs = args.vec_regs

    core.branchPred = TAGE_SC_L_8KB()

    # Functional units
    core.fuPool = DefaultFUPool()
    core.fuPool.FUList[0].count = args.int_alus  # int ALU
    core.fuPool.FUList[1].count = args.int_mds  # int mul/div
    core.fuPool.FUList[2].count = args.fp_alus  # fp ALU
    core.fuPool.FUList[3].count = args.fp_mds  # fp mul/div
    core.fuPool.FUList[8].count = args.mem_ports  # r/w mem port

    return o3_processor
