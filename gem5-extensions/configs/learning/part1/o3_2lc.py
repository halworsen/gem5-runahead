import m5
from m5.objects import *
import argparse

from caches import L1ICache, L1DCache, L2Cache

# system setup
def setup_system(binary, l1i='16kB', l1d='64kB', l2='256kB') -> System:
    system = System()

    clock = SrcClockDomain()
    clock.clock = '2GHz'
    clock.voltage_domain = VoltageDomain()
    system.clk_domain = clock

    system.mem_mode = 'timing'
    system.mem_ranges = [AddrRange('1GB')]

    # {Riscv/Arm/X86/Sparc/Power/Mips}{AtomicSimple/O3/TimingSimple/Kvm/Minor}CPU
    system.cpu = X86O3CPU()
    system.membus = SystemXBar()

    system.cpu.icache = L1ICache(l1i)
    system.cpu.dcache = L1DCache(l1d)
    # connect the cpu-side ports
    system.cpu.icache.connect(system.cpu)
    system.cpu.dcache.connect(system.cpu)

    system.l2bus = L2XBar()
    # connect memory-side ports
    system.cpu.icache.connect(system.l2bus)
    system.cpu.dcache.connect(system.l2bus)

    system.l2cache = L2Cache(l2)
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

    # process
    system.workload = SEWorkload.init_compatible(binary)

    # configure the CPU to use the binary as its workload
    process = Process()
    process.cmd = [binary]
    system.cpu.workload = process
    # create execution contexts which will execute the workload
    system.cpu.createThreads()

    return system


parser = argparse.ArgumentParser(description='Simple 2-level cache system')
parser.add_argument('binary', default='', nargs=1, type=str,
                    help='Binary to execute')
parser.add_argument('-i', '--l1i-size', default='16kB',
                    metavar='L1I size', help='L1 instruction cache size. Default: 16kB')
parser.add_argument('-d', '--l1d-size', default='64kB',
                    metavar='L1D size', help='L1 data cache size. Default: 64kB')
parser.add_argument('-s', '--l2-size', default='256kB',
                    metavar='L2 size', help='Shared L2 cache size. Default: 256kB')

opts = parser.parse_args()
opts.binary = opts.binary[0]

# start simulation
root = Root(
    full_system=False,  # SE mode
    system=setup_system(
        binary=opts.binary,
        l1i=opts.l1i_size,
        l1d=opts.l1d_size,
        l2=opts.l2_size,
    ),
)
m5.instantiate()

print('begin sim')
exit_event = m5.simulate()

print(f'Sim exited. t{m5.curTick()} - {exit_event.getCause()}')
