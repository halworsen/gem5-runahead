import m5
from m5.objects import *

# system setup
system = System()

clock = SrcClockDomain()
clock.clock = '2GHz'
clock.voltage_domain = VoltageDomain()
system.clk_domain = clock

system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('1GB')]

# {Riscv/Arm/X86/Sparc/Power/Mips}{AtomicSimple/O3/TimingSimple/Kvm/Minor}CPU
system.cpu = X86TimingSimpleCPU()

system.membus = SystemXBar()
system.cpu.icache_port = system.membus.cpu_side_ports
system.cpu.dcache_port = system.membus.cpu_side_ports

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
binary = '/home/gem5/gem5-runahead/gem5-extensions/configs/learning/hello'
system.workload = SEWorkload.init_compatible(binary)

# configure the CPU to use the binary as its workload
process = Process()
process.cmd = [binary]
system.cpu.workload = process
# create execution contexts which will execute the workload
system.cpu.createThreads()

# start simulation
root = Root(
    full_system = False,  # SE mode
    system = system,
)
m5.instantiate()

print('begin sim')
exit_event = m5.simulate()

print(f'Sim exited. t{m5.curTick()} - {exit_event.getCause()}')
