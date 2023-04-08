import m5
from m5.objects import *

from caches import L1ICache, L1DCache, L2Cache
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

    system.cpu = X86RunaheadCPU()
    #system.cpu.numROBEntries = 16
    system.membus = SystemXBar()

    system.cpu.icache = L1ICache('16kB')
    system.cpu.dcache = L1DCache('16kB')
    # connect the cpu-side ports
    system.cpu.icache.connect(system.cpu)
    system.cpu.dcache.connect(system.cpu)

    system.l2bus = L2XBar()
    # connect memory-side ports
    system.cpu.icache.connect(system.l2bus)
    system.cpu.dcache.connect(system.l2bus)

    system.l2cache = L2Cache('128kB')
    system.l2cache.connect(system.l2bus, side='cpu')
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
    process.cmd = [binary, str(args.size)]
    system.cpu.workload = process
    # create execution contexts which will execute the workload
    system.cpu.createThreads()

    return system

parser = argparse.ArgumentParser()

parser.add_argument('--size', type=int, default=32, help='Matrix size')

args = parser.parse_args()

# start simulation
root = Root(
    full_system=False,  # SE mode
    system=setup_system(args),
)
m5.instantiate()

print('begin sim')
exit_event = m5.simulate()

print(f'Sim exited @ t{m5.curTick()} - {exit_event.getCause()}')

