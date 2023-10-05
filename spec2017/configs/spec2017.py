import argparse
import os
from datetime import datetime

import m5
from m5.objects import Root

from gem5.utils.requires import requires
from gem5.components.boards.x86_board import X86Board
from gem5.isas import ISA
from gem5.resources.resource import CustomResource, CustomDiskImageResource

from core import setup_cores, add_core_args
from memory import setup_cache, setup_memory, add_memory_args
from options import add_parser_args

# Require X86 ISA
requires(isa_required=ISA.X86)

parser = argparse.ArgumentParser(
    prog=os.path.basename(__file__),
    description='Run SPEC2017 benchmarks in full system mode',
)

add_parser_args(parser)
add_core_args(parser)
add_memory_args(parser)

args = parser.parse_args()

print('Configuring system...')
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

print(f'Using linux kernel at: {args.kernel}')
print(f'Using disk image at: {args.image}')
print(f'Using readfile at: {args.script}')
print('Readfile contents:')
with open(args.script, 'r') as f:
    for i, line in enumerate(f.readlines()):
        print(f'\t{i+1} | {line.strip()}')

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

root = Root(full_system=True, system=board)

m5.instantiate()
m5.stats.reset()

print(f'Beginning simulation @ {datetime.now()}')
print('Performing boot...')

assert(root.system.readfile == args.script)

exit_event = m5.simulate()
tick = m5.curTick()
cause = exit_event.getCause()

# Boot completed successfully, dump stats and switch cores
if cause == 'm5_exit instruction encountered':
    print(f'Boot completed @ {datetime.now()}')
    print(f'Sim tick: t{tick}')
    print('Dumping and resetting simulation statistics...')

    m5.stats.dump()
    m5.stats.reset()

    # Switch to the detailed core (unless doing simpoint profiling)
    if args.simpoint_interval == 0:
        processor.switch()
# Something went wrong
else:
    print(f'Unexpected exit occured @ t{tick}')
    print(f'Exit cause: {cause}')
    exit(1)

print('Resuming simulation...')

exit_event = m5.simulate()
tick = m5.curTick()
cause = exit_event.getCause()

print(f'Finished simulation @ {datetime.now()}')
print(f'Exit cause: {cause}')

print('Dumping statistics')
m5.stats.dump()
