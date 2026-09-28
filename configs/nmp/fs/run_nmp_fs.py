import argparse
import os

import m5
from m5.objects import *
from m5.options import *
from m5.util import addToPath

m5.util.addToPath("../..")

import devices
from common import (
    ObjectList,
    SysPaths,
)
from common.cores.arm import (
    HPI,
    O3_ARM_v7a,
)

default_kernel = "vmlinux.arm64"
default_disk = "linaro-minimal-aarch64.img"
default_root_device = "/dev/vda1"


cpu_types = {
    "atomic": (AtomicSimpleCPU, None, None, None),
    "minor": (MinorCPU, devices.L1I, devices.L1D, devices.L2),
    "hpi": (HPI.HPI, HPI.HPI_ICache, HPI.HPI_DCache, HPI.HPI_L2),
    "o3": (
        O3_ARM_v7a.O3_ARM_v7a_3,
        O3_ARM_v7a.O3_ARM_v7a_ICache,
        O3_ARM_v7a.O3_ARM_v7a_DCache,
        O3_ARM_v7a.O3_ARM_v7aL2,
    ),
}

def create_cow_image(name):
    image = CowDiskImage()
    image.child.image_file = SysPaths.disk(name)
    return image

def create(args):
    if args.script and not os.path.isfile(args.script):
        print(f"Error: Bootscript {args.script} does not exist")
        sys.exit(1)

    cpu_class = cpu_types[args.cpu][0]
    mem_mode = cpu_class.memory_mode()
    want_caches = True if mem_mode == "timing" else False

    system = devices.SimpleSystem(
        want_caches,
        args.mem_size,
        mem_mode=mem_mode,
        workload=ArmFsLinux(object_file=SysPaths.binary(args.kernel)),
        readfile=args.script,
    )

    # PREVENT SVE KERNEL PANIC
    system.release = Armv8()

    system.pci_devices = [
        PciVirtIO(vio=VirtIOBlock(image=create_cow_image(args.disk_image)))
    ]

    for dev in system.pci_devices:
        system.attach_pci(dev)

    system.connect()

    system.cpu_cluster = [
        devices.ArmCpuCluster(
            system,
            args.num_cores,
            args.cpu_freq,
            "1.0V",
            *cpu_types[args.cpu],
            tarmac_gen=args.tarmac_gen,
            tarmac_dest=args.tarmac_dest,
        )
    ]

    system.nmp = NMPUnit(nmp_latency=5)
    system.nmp.nmp_range = AddrRange(start=0xA0000000, size=4096)

    # MANUALLY CONFIGURE DRAM AND NMP
    system.mem_ctrl = MemCtrl()
    system.mem_ctrl.dram = ObjectList.mem_list.get(args.mem_type)()
    system.mem_ctrl.dram.range = system.mem_ranges[0]

    if want_caches:
        system.addCaches(want_caches, last_cache_level=2)
        system.nmp.mem_side_port = system.mem_ctrl.port
        system.membus.mem_side_ports = system.nmp.cpu_side_port
    else:
        system.nmp.mem_side_port = system.mem_ctrl.port
        system.membus.mem_side_ports = system.nmp.cpu_side_port

    system.realview.setupBootLoader(system, SysPaths.binary)

    if args.dtb:
        system.workload.dtb_filename = args.dtb
    else:
        system.workload.dtb_filename = os.path.join(
            m5.options.outdir, "system.dtb"
        )
        system.generateDtb(system.workload.dtb_filename)

    if args.initrd:
        system.workload.initrd_filename = args.initrd

    kernel_cmd = [
        "console=ttyAMA0",
        "lpj=19988480",
        "norandmaps",
        f"root={args.root_device}",
        "rw",
        f"mem={args.mem_size}",
    ]
    system.workload.command_line = " ".join(kernel_cmd)

    if args.with_pmu:
        for cluster in system.cpu_cluster:
            interrupt_numbers = [args.pmu_ppi_number] * len(cluster)
            cluster.addPMUs(interrupt_numbers)

    return system


def run(args):
    cptdir = m5.options.outdir
    if args.checkpoint:
        print(f"Checkpoint directory: {cptdir}")

    while True:
        event = m5.simulate()
        exit_msg = event.getCause()
        if exit_msg == "checkpoint":
            print("Dropping checkpoint at tick %d" % m5.curTick())
            cpt_dir = os.path.join(m5.options.outdir, "cpt.%d" % m5.curTick())
            m5.checkpoint(os.path.join(cpt_dir))
            print("Checkpoint done. Exiting so you can restore with a detailed CPU!")
            break # We stop the simulation here!
        else:
            print(f"{exit_msg} ({event.getCode()}) @ {m5.curTick()}")
            break


def arm_ppi_arg(int_num: int) -> int:
    int_num = int(int_num)
    if 16 <= int_num <= 31:
        return int_num
    raise ValueError(f"{int_num} is not a valid Arm PPI number")


def main():
    parser = argparse.ArgumentParser(epilog=__doc__)
    parser.add_argument("--dtb", type=str, default=None, help="DTB file to load")
    parser.add_argument("--kernel", type=str, default=default_kernel, help="Linux kernel")
    parser.add_argument("--initrd", type=str, default=None, help="initrd/initramfs file to load")
    parser.add_argument("--disk-image", type=str, default=default_disk, help="Disk to instantiate")
    parser.add_argument("--root-device", type=str, default=default_root_device, help=f"OS device name for root partition")
    parser.add_argument("--script", type=str, default="", help="Linux bootscript")
    parser.add_argument("--cpu", type=str, choices=list(cpu_types.keys()), default="atomic", help="CPU model to use")
    parser.add_argument("--cpu-freq", type=str, default="4GHz")
    parser.add_argument("--num-cores", type=int, default=1, help="Number of CPU cores")
    parser.add_argument("--mem-type", default="DDR3_1600_8x8", choices=ObjectList.mem_list.get_names(), help="type of memory to use")
    parser.add_argument("--mem-channels", type=int, default=1, help="number of memory channels")
    parser.add_argument("--mem-ranks", type=int, default=None, help="number of memory ranks per channel")
    parser.add_argument("--mem-size", action="store", type=str, default="2GiB", help="Specify the physical memory size")
    parser.add_argument("--tarmac-gen", action="store_true", help="Write a Tarmac trace.")
    parser.add_argument("--tarmac-dest", choices=TarmacDump.vals, default="stdoutput", help="Destination for the Tarmac trace output.")
    parser.add_argument("--with-pmu", action="store_true", help="Add a PMU to each core in the cluster.")
    parser.add_argument("--pmu-ppi-number", type=arm_ppi_arg, default=23, help="PPI Number")
    parser.add_argument("--checkpoint", action="store_true")
    parser.add_argument("--restore", type=str, default=None)

    args = parser.parse_args()

    root = Root(full_system=True)
    root.system = create(args)

    if args.restore is not None:
        m5.instantiate(args.restore)
    else:
        m5.instantiate()

    run(args)


if __name__ == "__m5_main__":
    main()