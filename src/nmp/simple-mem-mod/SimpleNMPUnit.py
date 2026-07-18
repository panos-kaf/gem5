from m5.params import *
from m5.objects.SimpleMemory import SimpleMemory

class SimpleNMPUnit(SimpleMemory):
    type = 'SimpleNMPUnit'
    cxx_header = 'nmp/demo2/simple_nmp_unit.hh'
    cxx_class = 'gem5::nmp::SimpleNMPUnit'


