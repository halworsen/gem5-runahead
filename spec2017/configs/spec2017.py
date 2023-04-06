import argparse
import os

import m5
from m5.objects import Root

from gem5.utils.requires import requires
from gem5.components.boards.x86_board import X86Board
from gem5.components.boards.mem_mode import MemMode
from gem5.components.processors.cpu_types import CPUTypes
from gem5.isas import ISA
from gem5.resources.resource import CustomResource, CustomDiskImageResource

from core import setup_cores, add_core_args
from memory import setup_cache, setup_memory, add_memory_args
from options import add_parser_args

# Require X86 ISA and KVM capabilities
# KVM is used to "fast-forward" through the Linux boot
requires(
    isa_required=ISA.X86,
    kvm_required=True,
)

parser = argparse.ArgumentParser(
    prog=os.path.basename(__file__),
    description='Run SPEC2017 benchmarks in full system mode',
)

add_parser_args(parser)
add_core_args(parser)
add_memory_args(parser)

args = parser.parse_args()

processor = setup_cores(args)
board = X86Board(
    clk_freq=args.clock,
    processor=processor,
    cache_hierarchy=setup_cache(args),
    memory=setup_memory(args),
)

# Check that the kernel, image and runscript exist
assert os.path.exists(args.kernel)
assert os.path.exists(args.image)
assert os.path.exists(args.script)

board.set_kernel_disk_workload(
    kernel=CustomResource(args.kernel),
    disk_image=CustomDiskImageResource(
        local_path=args.image,
        disk_root_partition='1',
    ),
    readfile=args.script,
)


# Apparently we need this for long running processes.
m5.disableAllListeners()

root = Root(full_system = True, system = board)

m5.instantiate()
m5.stats.reset()

print('Beginning simulation')
print('Performing boot...')

exit_event = m5.simulate()
tick = m5.curTick()
cause = exit_event.getCause()

# Boot completed successfully, dump stats
if cause == 'm5_exit instruction encountered':
    print(f'Boot completed @ t{tick}')
    print('Dumping and resetting simulation statistics...')

    m5.stats.dump()
    m5.stats.reset()

    # Set the max instruction count so that we go for that amount in the actual ROI
    core = board.get_processor().get_cores()[0].core
    core.max_insts_any_thread = core.totalInsts() + args.max_insts
# Something went wrong
else:
    print('Unexpected exit occured @ t{tick}')
    print(f'Exit cause: {cause}')
    exit(1)

print('Resuming simulation...')

exit_event = m5.simulate()
tick = m5.curTick()
cause = exit_event.getCause()

print('Finished simulation')
print(f'Exit cause: {cause}')

print('Dumping m5 statistics')
m5.stats.dump()
