#include "nmp_unit.hh"

#include "mem/packet_access.hh" 
#include "debug/NMPUnit.hh" // You can create a debug flag later to track this!

#include <iostream>

namespace gem5
{

namespace nmp
{

NMPUnit::NMPUnit(const NMPUnitParams &p) :
    ClockedObject(p),
    cpuPort(p.name + ".cpu_side_port", this),
    memPort(p.name + ".mem_side_port", this),
    nmpRange(p.nmp_range),
    nmpLatency(p.nmp_latency),
    processEvent([this]{ processResponse(); }, p.name)
{
    std::cout << "NMPUnit constructed!\n";
}

Port&
NMPUnit::getPort(const std::string &if_name, PortID idx)
{
    if (if_name == "cpu_side_port") {
        return cpuPort;
    } else if (if_name == "mem_side_port") {
        return memPort;
    } else {
        return ClockedObject::getPort(if_name, idx);
    }
}

AddrRangeList
NMPUnit::CPUSidePort::getAddrRanges() const
{
    // Ask the actual memory attached to our memPort what addresses it handles,
    // and pass that information back up to the CPU side!
    return owner->memPort.getAddrRanges();
}



// TRAFFIC FROM CPU TO MEM

void
NMPUnit::CPUSidePort::recvRespRetry()
{
    owner->memPort.sendRetryResp();
}

bool
NMPUnit::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    // blindly forward the request straight to memory
    
    //std::cout << "recvTimingReq called\n";
    
    // std::cout << "REQ ";// << std::hex << pkt->getAddr();
    // //   << " tick " << curTick() << std::endl;
    
    // std::cout << //(pkt->isInstFetch() ? "IFETCH " :
    // (pkt->isRead() ? "READ " :
    // pkt->isWrite() ? "WRITE " : "OTHER ")
    // << std::hex << pkt->getAddr()
    // << std::endl;
    
    return owner->memPort.sendTimingReq(pkt);
}

// TRAFFIC FROM MEM TO CPU

void
NMPUnit::MemSidePort::recvRangeChange()
{
    owner->cpuPort.sendRangeChange();
}

void
NMPUnit::MemSidePort::recvReqRetry()
{
    owner->cpuPort.sendRetryReq();
}

bool
NMPUnit::MemSidePort::recvTimingResp(PacketPtr pkt)
{

    // TEMPORARY: Just forward the response immediately for now.
    return owner->cpuPort.sendTimingResp(pkt);

    // std::cout << "RESP ";// << std::hex << pkt->getAddr()
    //     //   << " tick " << curTick() << std::endl;

    // std::cout << //(pkt->isInstFetch() ? "IFETCH " :
    // (pkt->isRead() ? "READ " :
    // pkt->isWrite() ? "WRITE " : "OTHER ")
    // << std::hex << pkt->getAddr()
    // << std::endl;
    

    // Not an NMP packet? Just forward it immediately.
    if (!owner->nmpRange.contains(pkt->getAddr())) {
        return owner->cpuPort.sendTimingResp(pkt);
    }

    // Only intercept reads.
    if (!pkt->isRead()) {
        return owner->cpuPort.sendTimingResp(pkt);
    }

    // Only handle 32-bit accesses.
    if (pkt->getSize() != sizeof(uint32_t)) {
        return owner->cpuPort.sendTimingResp(pkt);
    }

    // DPRINTF(NMPUnit, "Received NMP response at tick %llu\n", curTick());

    std::cout << "[NMP] Received response at tick " << curTick() << std::endl;

    // Store the packet for later.
    owner->pendingPkt = pkt;

    // Schedule processing after the NMP latency.
    owner->schedule(owner->processEvent,
                    curTick() + owner->nmpLatency * owner->clockPeriod());

    // We accepted the response, but we will forward it later.
    return true;
}

void
NMPUnit::processResponse()
{

    PacketPtr pkt = pendingPkt;

    std::cout << "PROCESS "; //<< std::hex << pkt->getAddr()
            //   << " tick " << curTick() << std::endl;

    std::cout << //(pkt->isInstFetch() ? "IFETCH " :
    (pkt->isRead() ? "READ " :
    pkt->isWrite() ? "WRITE " : "OTHER ")
    << std::hex << pkt->getAddr()
    << std::endl;

    uint32_t data = pkt->getLE<uint32_t>();

    std::cout << "[NMP] Processing response at tick " << curTick() << std::endl;

    if (data == 0xBABEBABE) {
        data = 0xCAFECAFE;
        pkt->setLE<uint32_t>(data);
    }

    if (!cpuPort.sendTimingResp(pkt)) {
        // If the CPU port couldn't accept the response, we might need to handle that case.
        // For simplicity, we will just print a message here.
        std::cerr << "[NMP] Warning: CPU port could not accept the response packet!" << std::endl;
    }

    pendingPkt = nullptr;
}

} // namespace nmp
} // namespace gem5
