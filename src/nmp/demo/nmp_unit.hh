#ifndef __NMP_DEMO_NMP_UNIT_HH__
#define __NMP_DEMO_NMP_UNIT_HH__

#include "mem/port.hh"
#include "params/NMPUnit.hh"
#include "sim/clocked_object.hh"

namespace gem5
{

namespace nmp
{

class NMPUnit : public ClockedObject
{
  private:

    // The port facing the CPU
    class CPUSidePort : public ResponsePort
    {
      private:
        NMPUnit *owner;

      protected:
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
        AddrRangeList getAddrRanges() const override;

      public:
        CPUSidePort(const std::string& name, NMPUnit *owner) : ResponsePort(name), owner(owner) {}
        Tick recvAtomic(PacketPtr pkt) override { return owner->memPort.sendAtomic(pkt); }
        void recvFunctional(PacketPtr pkt) override { owner->memPort.sendFunctional(pkt); }
    };

    // The port facing the Memory
    class MemSidePort : public RequestPort
    {
      private:
        NMPUnit *owner;

      protected:
        bool recvTimingResp(PacketPtr pkt) override;
        void recvReqRetry() override;
        void recvRangeChange() override;

      public:
        MemSidePort(const std::string& name, NMPUnit *owner) : RequestPort(name), owner(owner) {}
    };

    CPUSidePort cpuPort;
    MemSidePort memPort;

    // NMP PARAMETERS
    AddrRange nmpRange;
    Cycles nmpLatency;


    PacketPtr pendingPkt = nullptr;

    void processResponse();

    EventFunctionWrapper processEvent;

  public:
    NMPUnit(const NMPUnitParams &p);
    Port &getPort(const std::string &if_name, PortID idx=InvalidPortID) override;
};

} // namespace nmp
} // namespace gem5

#endif // __NMP_DEMO_NMP_UNIT_HH__
