import m5
from m5.objects import *

from caches import L1ICache, L1DCache, L2Cache, L3Cache
import argparse

# system setup
def setup_system(args) -> System:
    system = System()

    clock = SrcClockDomain()
    clock.clock = '2GHz'
    clock.voltage_domain = VoltageDomain()
    system.clk_domain = clock

    system.mem_mode = 'timing'
    system.mem_ranges = [AddrRange('1GB')]

    #system.cpu = X86O3CPU()
    system.cpu = X86RunaheadCPU()
    system.cpu.enableRunahead = True
    system.membus = SystemXBar()

    system.cpu.icache = L1ICache('32kB')
    system.cpu.dcache = L1DCache('32kB')
    system.l2bus = L2XBar()
    system.l2cache = L2Cache('256kB')

    # connect L1 cache to the cpu-side ports
    system.cpu.icache.connect(system.cpu)
    system.cpu.dcache.connect(system.cpu)

    # connect L1 caches to the L2 crossbar
    system.cpu.icache.connect(system.l2bus)
    system.cpu.dcache.connect(system.l2bus)
    
    # connect L2 cache to L2 crossbar for CPU-side comms
    system.l2cache.connect(system.l2bus, side='cpu')

    if args.l3:
        print('adding L3 cache to system')
        system.l3cache = L3Cache('6MB')

        # connect L3 cache to L2 and memory bus
        # L3 -> L2 is a reciprocal connection, don't need to attach L2 memside to L3
        system.l3cache.connect(system.l2cache, side='cpu')
        system.l3cache.connect(system.membus, side='mem')
    else:
        print('connecting L2 to system memory bus')
        # connect L2 to memory bus
        system.l2cache.connect(system.membus, side='mem')


    system.cpu.createInterruptController()
    system.cpu.interrupts[0].pio = system.membus.mem_side_ports
    system.cpu.interrupts[0].int_requestor = system.membus.cpu_side_ports
    system.cpu.interrupts[0].int_responder = system.membus.mem_side_ports

    system.system_port = system.membus.cpu_side_ports

    system.mem_ctrl = MemCtrl()
    system.mem_ctrl.dram = DDR4_2400_8x8()
    system.mem_ctrl.dram.range = system.mem_ranges[0]
    system.mem_ctrl.port = system.membus.mem_side_ports

    # setup the binary, a nxn 64bit int matrix multiplication
    # this should barely be over 
    binary = '/cluster/home/markuswh/gem5-runahead/gem5-extensions/configs/test/matmul'
    system.workload = SEWorkload.init_compatible(binary)
    process = Process()
    process.cmd = [binary, str(args.size), str(args.random)]
    system.cpu.workload = process
    # create execution contexts which will execute the workload
    system.cpu.createThreads()

    return system

parser = argparse.ArgumentParser()

parser.add_argument('--size', type=int, default=32, help='Matrix size')
parser.add_argument('--random', type=int, default=32, help='Shuffle multiplication')
parser.add_argument('--l3', default=True, action='store_true')
parser.add_argument('--no-l3', dest='l3', action='store_false', help='Use a two-level cache hierarchy')

args = parser.parse_args()

# start simulation
sys = setup_system(args)
root = Root(
    full_system=False,  # SE mode
    system=sys,
)
m5.instantiate()

print(f'begin sim. size: {args.size}, random: {"yes" if (args.random == 1) else "no"}, runahead: {"enabled" if sys.cpu.enableRunahead else "disabled"}')
exit_event = m5.simulate()

print(f'Sim exited @ t{m5.curTick()} - {exit_event.getCause()}')

