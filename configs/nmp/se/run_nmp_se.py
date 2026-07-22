import m5
from m5.objects import *
import os

# Create the base system
system = System()

# Set up the clock (1 GHz) and voltage
system.clk_domain = SrcClockDomain()
system.clk_domain.clock = '1GHz'
system.clk_domain.voltage_domain = VoltageDomain()

# Use timing mode for cycle-accurate simulation
system.mem_mode = 'timing'
system.mem_ranges = [AddrRange('8192MB')]

# Create a Simple In-Order CPU
system.cpu = TimingSimpleCPU()

# Create the Memory Bus (Crossbar)
system.membus = NoncoherentXBar()

system.membus.width = 16              # 16 bytes (128 bits) wide
system.membus.frontend_latency = 3    # 3 cycles to accept a request
system.membus.forward_latency = 4     # 4 cycles to route the request
system.membus.response_latency = 2    # 2 cycles to route the response

system.system_port = system.membus.cpu_side_ports

# Create and wire the NMP unit

system.nmp = NMPUnit(nmp_latency=1000)

# Target specific address for NMP
system.nmp.nmp_range = AddrRange(start=0x80000000, size=4096)

# Set up the workload (Our compiled C program)
binary = '../workloads/nmp/test_nmp'
if not os.path.exists(binary):
    print(f"Error: Could not find '{binary}'. Please compile test_nmp.c first!")
    exit(1)

system.workload = SEWorkload.init_compatible(binary)

process = Process()
process.cmd = [binary]

# Assign the process to the CPU workload
system.cpu.workload = process
system.cpu.createThreads()

# Wire the Instruction port directly to the bus (bypassing NMP for instructions)
system.cpu.icache_port = system.membus.cpu_side_ports

# Wire the Data port THROUGH the NMP unit
# CPU <---> NMP <---> MemBus
system.cpu.dcache_port = system.nmp.cpu_side_port
system.nmp.mem_side_port = system.membus.cpu_side_ports

# Helper function required for x86/ARM to connect CPU interrupts
system.cpu.createInterruptController()

# Create the physical memory (RAM) and connect it to the bus
system.mem_ctrl = MemCtrl()
system.mem_ctrl.dram = DDR3_1600_8x8()
system.mem_ctrl.dram.range = system.mem_ranges[0]
system.mem_ctrl.port = system.membus.mem_side_ports

# Define the root
root = Root(full_system=False, system=system)

# Start the simulation
print("Instantiating C++ simulation objects...")
m5.instantiate()

# Map the memory AFTER instantiation so the C++ Process object exists
process.map(0x80000000, 0x80000000, 4096)

print("Beginning NMP simulation!")
exit_event = m5.simulate()
print('Exiting @ tick {} because {}'.format(m5.curTick(), exit_event.getCause()))
