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
    // 1. Not an NMP packet or not a read? Just forward it immediately.
    if (!owner->nmpRange.contains(pkt->getAddr()) || !pkt->isRead()) {
        return owner->cpuPort.sendTimingResp(pkt);
    }

    // 2. We removed the early return AND the strict size check!
    // If we reach here, it is a read packet that overlaps with our NMP range.
    std::cout << "[NMP DEBUG] Intercepted NMP Read at Addr: 0x" << std::hex << pkt->getAddr() 
              << " | Size: " << std::dec << pkt->getSize() << " bytes\n";

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

    // Ensure the packet actually has a data payload we can read
    if (pkt->hasData()) {
        
        // Find exactly where our target address (0xA0000000) lives inside this packet.
        // A packet might start exactly at 0xA0000000, or it might be a 64-byte block 
        // starting earlier (e.g., 0x9FFFFFC0) that simply CONTAINS our address.
        Addr pkt_start = pkt->getAddr();
        Addr target_addr = 0xA0000000;

        // Check if our target address is actually inside this packet's data payload
        if (target_addr >= pkt_start && target_addr < (pkt_start + pkt->getSize())) {
            
            // Calculate the exact byte offset inside the payload
            Addr offset = target_addr - pkt_start; 
            
            // Cast the packet data payload to a byte array so we can offset into it
            uint8_t *payload = pkt->getPtr<uint8_t>();
            
            // Read the specific 32-bit chunk
            uint32_t data = *(uint32_t*)(payload + offset);

            if (data == 0xBABEBABE) {
                std::cout << "[NMP DEBUG] Found 0xBABEBABE inside payload! Replacing with 0xCAFECAFE...\n";
                *(uint32_t*)(payload + offset) = 0xCAFECAFE;
            } else {
                std::cout << "[NMP DEBUG] Target offset contained: 0x" << std::hex << data << " (Not a match)\n";
            }
        }
    }

    if (!cpuPort.sendTimingResp(pkt)) {
        std::cerr << "[NMP] Warning: CPU port could not accept the response packet!" << std::endl;
    }

    pendingPkt = nullptr;
}

/*
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
*/}

} // namespace nmp
} // namespace gem5
