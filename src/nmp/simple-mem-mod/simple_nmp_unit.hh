/**
 * @file
 * SimpleNMPUnit declaration
 */

#ifndef __MEM_SIMPLE_NMP_UNIT_HH__
#define __MEM_SIMPLE_NMP_UNIT_HH__

#include <list>

#include "base/random.hh"
#include "mem/abstract_mem.hh"
#include "mem/port.hh"
#include "params/SimpleNMPUnit.hh"

namespace gem5
{

namespace nmp
{

/**
 * The simple memory is a basic single-ported memory controller with
 * a configurable throughput and latency.
 *
 * @sa  \ref gem5MemorySystem "gem5 Memory System"
 */
class SimpleNMPUnit : public memory::AbstractMemory
{

  private:

    /**
     * A deferred packet stores a packet along with its scheduled
     * transmission time
     */
    class DeferredPacket
    {

      public:

        const Tick tick;
        const PacketPtr pkt;

        DeferredPacket(PacketPtr _pkt, Tick _tick) : tick(_tick), pkt(_pkt)
        { }
    };

    class MemoryPort : public ResponsePort
    {
      private:
        SimpleNMPUnit& mem;

      public:
        MemoryPort(const std::string& _name, SimpleNMPUnit& _nmp_unit);

      protected:
        Tick recvAtomic(PacketPtr pkt) override;
        Tick recvAtomicBackdoor(
                PacketPtr pkt, MemBackdoorPtr &_backdoor) override;
        void recvFunctional(PacketPtr pkt) override;
        void recvMemBackdoorReq(const MemBackdoorReq &req,
                MemBackdoorPtr &backdoor) override;
        bool recvTimingReq(PacketPtr pkt) override;
        void recvRespRetry() override;
        AddrRangeList getAddrRanges() const override;
    };

    MemoryPort port;

    /**
     * Latency from that a request is accepted until the response is
     * ready to be sent.
     */
    const Tick latency;

    /**
     * Fudge factor added to the latency.
     */
    const Tick latency_var;

    /**
     * Internal (unbounded) storage to mimic the delay caused by the
     * actual memory access. Note that this is where the packet spends
     * the memory latency.
     */
    std::list<DeferredPacket> packetQueue;

    /**
     * Bandwidth in ticks per byte. The regulation affects the
     * acceptance rate of requests and the queueing takes place after
     * the regulation.
     */
    const double bandwidth;

    /**
     * Track the state of the memory as either idle or busy, no need
     * for an enum with only two states.
     */
    bool isBusy;

    /**
     * Remember if we have to retry an outstanding request that
     * arrived while we were busy.
     */
    bool retryReq;

    /**
     * Remember if we failed to send a response and are awaiting a
     * retry. This is only used as a check.
     */
    bool retryResp;

    mutable Random::RandomPtr rng = Random::genRandom();

    /**
     * Release the memory after being busy and send a retry if a
     * request was rejected in the meanwhile.
     */
    void release();

    EventFunctionWrapper releaseEvent;

    /**
     * Dequeue a packet from our internal packet queue and move it to
     * the port where it will be sent as soon as possible.
     */
    void dequeue();

    EventFunctionWrapper dequeueEvent;

    /**
     * Detemine the latency.
     *
     * @return the latency seen by the current packet
     */
    Tick getLatency() const;

    /**
     * Upstream caches need this packet until true is returned, so
     * hold it for deletion until a subsequent call
     */
    std::unique_ptr<Packet> pendingDelete;

  public:

    SimpleNMPUnit(const SimpleNMPUnitParams &p);

    DrainState drain() override;

    Port &getPort(const std::string &if_name,
                  PortID idx=InvalidPortID) override;
    void init() override;

  protected:
    Tick recvAtomic(PacketPtr pkt);
    Tick recvAtomicBackdoor(PacketPtr pkt, MemBackdoorPtr &_backdoor);
    void recvFunctional(PacketPtr pkt);
    void recvMemBackdoorReq(const MemBackdoorReq &req,
            MemBackdoorPtr &backdoor);
    bool recvTimingReq(PacketPtr pkt);
    void recvRespRetry();
};

} // namespace memory
} // namespace gem5

#endif //__MEM_SIMPLE_NMP_UNIT_HH__
