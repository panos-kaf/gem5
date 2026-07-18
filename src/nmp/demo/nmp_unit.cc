#include "nmp_unit.hh"
#include <iostream>
#include "mem/packet_access.hh" 
#include "debug/NMPUnit.hh" // You can create a debug flag later to track this!

namespace gem5
{

namespace nmp
{

NMPUnit::NMPUnit(const NMPUnitParams &p) :
    ClockedObject(p),
    cpuPort(p.name + ".cpu_side_port", this),
    memPort(p.name + ".mem_side_port", this),
    nmpRange(p.nmp_range),
    nmpLatency(p.nmp_latency)
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
bool
NMPUnit::CPUSidePort::recvTimingReq(PacketPtr pkt)
{
    // blindly forward the request straight to memory
    //std::cout << "recvTimingReq called\n";
    return owner->memPort.sendTimingReq(pkt);
}

// TRAFFIC FROM MEM TO CPU
/*
bool
NMPUnit::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Do NMP if addr is in nmpRange
    if (owner->nmpRange.contains(pkt->getAddr())) {
        
        // IS IT A READ OF THE RIGHT SIZE?
        if (pkt->isRead() && pkt->getSize() == sizeof(uint32_t)) {
            
            // NMP OP
            uint32_t data = pkt->getLE<uint32_t>();
	    if (data == 0xBABEBABE){
		data = 0xCAFECAFE;
            	pkt->setLE<uint32_t>(data);

            // Note: Don't send instantly, schedule an Event here for 
            // curTick() + (nmpLatency * clockPeriod()) instead
        }
	}
    }

    // Forward the packet back to the CPU
    return owner->cpuPort.sendTimingResp(pkt);
}
*/

bool
NMPUnit::MemSidePort::recvTimingResp(PacketPtr pkt)
{

    if (owner->nmpRange.contains(pkt->getAddr())) {

        // DEBUG PRINT
        std::cout << "\n[NMP HARDWARE] Intercepted packet at 0x" << std::hex << pkt->getAddr()
                  << "\n  - isRead: " << pkt->isRead()
                  << "\n  - Size: " << std::dec << pkt->getSize() << " bytes\n";

        // If your bus padded the read to 8 bytes, change sizeof(uint32_t) to 8 here
        if (pkt->isRead() && pkt->getSize() == sizeof(uint32_t)) {

            uint32_t data = pkt->getLE<uint32_t>();
            std::cout << "  - Data inside: 0x" << std::hex << data << "\n";

            if (data == 0xBABEBABE) {
                std::cout << "  - MATCH! Replacing with 0xCAFECAFE...\n\n";
                data = 0xCAFECAFE;
                pkt->setLE<uint32_t>(data);

                // (Your delay scheduling logic goes here)
                //return true;
		return owner->cpuPort.sendTimingResp(pkt);
            }
        }
    }

    return owner->cpuPort.sendTimingResp(pkt);
}

/*
bool
NMPUnit::MemSidePort::recvTimingResp(PacketPtr pkt)
{
    // Do NMP if addr is in nmpRange
    if (owner->nmpRange.contains(pkt->getAddr())) {

        // IS IT A READ OF THE RIGHT SIZE?
        if (pkt->isRead() && pkt->getSize() == sizeof(uint32_t)) {

            // NMP OP
            uint32_t data = pkt->getLE<uint32_t>();

            // FIXED: Added closing parenthesis and brackets
            if (data == 0xBABEBABE) {
                data = 0xCAFECAFE;
                pkt->setLE<uint32_t>(data);

                // --- IMPLEMENTING THE DELAY ---

                // Calculate the delay in gem5 Ticks
                Tick delay = owner->nmpLatency * owner->clockPeriod();

                // Create a one-off event using a lambda function to send the response later
                Event* delayedRespEvent = new EventFunctionWrapper(
                    [this, pkt]{
                        // This lambda runs in the future
                        owner->cpuPort.sendTimingResp(pkt);
                    },
                    "NMP delayed response",
                    true // true tells gem5 to auto-delete this event from memory after it fires
                );

                // Schedule the event on the simulator's timeline
                owner->schedule(delayedRespEvent, curTick() + delay);

                // Return true to the memory controller *now* so it knows we accepted the packet.
                // We will deal with sending it to the CPU in the future.
                return true;
            }
        }
    }

    // If it's not our special NMP packet (or not in range), forward it back to the CPU instantly
    return owner->cpuPort.sendTimingResp(pkt);
}
*/

} // namespace nmp
} // namespace gem5
