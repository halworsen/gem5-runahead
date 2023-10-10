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
from simpoints import parse_simpoints
import simulate

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

if args.restore_checkpoint:
    m5.instantiate(args.restore_checkpoint)
else:
    m5.instantiate()
m5.stats.reset()

print(f'Beginning simulation @ {datetime.now()}')

if args.simpoint_checkpoints:
    exit_event, ticks, cause = simulate.sim_fs_simpoint_checkpoints(root, args)
elif args.simpoint_interval:
    # Processor setup will have inserted the simpoint probe. Simulate as normal (on the simple core)
    exit_event, ticks, cause = simulate.sim_fs_normal(root, args, switch_core=False)
else:
    exit_event, ticks, cause = simulate.sim_fs_normal(root, args, switch_core=True)

print(f'Finished simulation @ {datetime.now()}')
print(f'Final exit cause: {cause}')

print('Dumping statistics')
m5.stats.dump()
