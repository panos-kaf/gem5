from m5.params import *
from m5.proxy import *
from m5.objects.ClockedObject import ClockedObject

class NMPUnit(ClockedObject):
    type = 'NMPUnit'
    cxx_header = "nmp/demo/nmp_unit.hh"
    cxx_class = 'gem5::nmp::NMPUnit'

    # the two ports to sit on the wire
    cpu_side_port = ResponsePort("Port connected to the CPU side")
    mem_side_port = RequestPort("Port connected to the memory side")

    # Any traffic in this range triggers the NMP math.
    nmp_range = Param.AddrRange("Address range where NMP is active")

    nmp_latency = Param.Cycles(5, "Compute delay for NMP operations")
