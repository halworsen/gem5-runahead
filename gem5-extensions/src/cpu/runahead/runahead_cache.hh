#ifndef _CPU_RUNAHEAD_RUNAHEAD_CACHE_HH__
#define _CPU_RUNAHEAD_RUNAHEAD_CACHE_HH__

#include <vector>

#include "base/types.hh"
#include "base/statistics.hh"
#include "mem/packet.hh"

namespace gem5
{
namespace runahead
{

/**
 * The runahead cache is a very simplified direct-mapped cache model residing in the CPU.
 * It's purpose is simply to cache runahead stores, as they are speculative even
 * at (pseudo)retirement, so we never allow them to actually writeback data to the cache.
 * The runahead cache does not model much of anything. It's essentially a glorified list.
 * 
 * Speculative runahead loads lookup the runahead cache before making requests to cache.
 * If there's a valid cache block in the runahead cache, this data is used. Note that a
 * runahead cache block may be valid while the stored data is poisoned. If a load uses a valid
 * cache block that contains poisoned data, the load is poisoned. If the load tries to get
 * an invalid cache block, that just means the load didn't depend on a runahead store, so
 * the request goes to normal cache.
*/
class RunaheadCache
{
private:
    struct CacheBlock
    {
        uint8_t *data;
        uint64_t tag;
        bool valid;
        bool poisoned;

        CacheBlock(uint8_t *data, uint64_t tag, bool valid, bool poisoned) :
            data(data), tag(tag), valid(valid), poisoned(poisoned) {};
    };

    /** Size in bytes of the entire cache */
    const uint64_t size;
    /** Size in bytes of each cache block */
    const uint8_t blockSize;
    /** The total amount of cache blocks */
    const uint64_t numBlocks;

    std::vector<CacheBlock> cacheEntries;

    /** Various bitshifts/masks of the address */
    const uint indexShift;
    const uint indexMask;
    const uint tagShift;

    /** Extract the block index from an address */
    uint64_t getIndex(Addr addr) { return (addr >> indexShift) & indexMask; };

    /** Extract the tag from an address */
    uint64_t getTag(Addr addr) { return (addr >> tagShift); };

    /** Align an address to its cache block boundary. */
    Addr align(Addr addr) { return (addr - addr % blockSize); };

    /** Get a block from the runahead cache, given that it is in the cache */
    CacheBlock *getBlock(Addr addr);

    /**
     * Check if data is in cache.
     * That is, the cache block's tag matches the address and the block is valid.
     */
    bool lookup(Addr addr);

    /**
     * Write some data to the runahead cache.
     * Conflicts are ignored, the "eviction policy" is to simply overwrite the block.
     */
    void write(PacketPtr pkt);

    /** Read some data from runahead cache. */
    uint8_t *read(PacketPtr pkt);

public:
    /** Sizes should be in bytes */
    RunaheadCache(statistics::Group *statsParent, uint64_t size, uint8_t blockSize);

    ~RunaheadCache();

    /** Poison a block associated with an address. Does nothing on a tag mismatch. */
    void poisonblock(Addr addr);

    /** Invalidates all cache blocks. */
    void invalidateCache();

    /** Process an incoming packet. */
    bool handlePacket(PacketPtr pkt);

private:
    struct RCacheStats : public statistics::Group
    {
        RCacheStats(statistics::Group *parent);

        // number of block lookups
        statistics::Scalar lookups;
        // number of writes to any block
        statistics::Scalar writes;
        // number of writes that resulted in a conflict
        statistics::Scalar writeConflicts;
        // number of poisoned writes to rcache
        statistics::Scalar poisonedWrites;
        // number of writes that cleansed poison by writing clean data to cache
        statistics::Scalar writeCleanses;
        // number of read misses
        statistics::Scalar readMisses;
        // number of read hits
        statistics::Scalar readHits;
        // number of times a cache block was poisoned by having poisoned data written to it
        statistics::Scalar poisons;
        // number of times rcache was invalidated
        statistics::Scalar invalidations;
        // number of packets served by rcache
        statistics::Scalar packetsHandled;
    } rcacheStats;
};

} // namespace runahead
} // namespace gem5

#endif // _CPU_RUNAHEAD_RUNAHEAD_CACHE_HH__