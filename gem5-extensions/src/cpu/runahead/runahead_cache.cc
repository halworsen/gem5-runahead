#include "cpu/runahead/runahead_cache.hh"
#include "cpu/runahead/lsq.hh"
#include "base/intmath.hh"
#include "base/trace.hh"
#include "debug/RCache.hh"

namespace gem5
{
namespace runahead
{

using LSQRequest = LSQ::LSQRequest;

RunaheadCache::RunaheadCache(statistics::Group *statsParent, uint64_t size, uint8_t blockSize)
    : size(size), blockSize(blockSize),
    numBlocks(size / blockSize),
    indexShift(ceilLog2(blockSize)),
    // instead of pow2
    indexMask((1 << ceilLog2(numBlocks)) - 1),
    tagShift(indexShift + ceilLog2(numBlocks)),
    rcacheStats(statsParent)
{
    // not a disaster since we use ceiling log2, but it does lead to some wasted bit real estate
    warn_if(!isPowerOf2(numBlocks), "Amount of runahead cache blocks should be a power of 2! Check cache size.\n");

    for (int idx = 0; idx < numBlocks; idx++) {
        cacheEntries.emplace_back(new uint8_t[blockSize], (uint64_t)0, false, false);
    }
}

RunaheadCache::~RunaheadCache()
{
    for (CacheBlock &block : cacheEntries) {
        delete[] block.data;
    }
}

RunaheadCache::CacheBlock*
RunaheadCache::getBlock(Addr addr)
{
    uint64_t idx = getIndex(addr);
    CacheBlock block = cacheEntries[idx];
    if (block.tag == getTag(addr)) {
        return &cacheEntries[idx];
    }

    return nullptr;
}

bool
RunaheadCache::lookup(Addr addr)
{
    uint64_t idx = getIndex(addr);
    CacheBlock block = cacheEntries[idx];

    DPRINTF(RCache, "R-cache lookup on block %llu (addr %#x). Tag: %i, valid: %i, poisoned: %i\n",
            idx, align(addr), (block.tag == getTag(addr)), block.valid, block.poisoned);

    ++rcacheStats.lookups;
    return ((block.tag == getTag(addr)) && block.valid);
}

void
RunaheadCache::write(PacketPtr pkt)
{
    assert(pkt->isWrite());

    Addr addr = pkt->getAddr();
    DPRINTF(RCache, "Performing R-cache write to block %llu (addr %#x, unaligned %#x).",
            getIndex(addr), align(addr), addr);

    CacheBlock &block = cacheEntries[getIndex(addr)];
    if ((block.tag != getTag(addr)) && block.valid) {
        DPRINTF(RCache, "Write conflicted. Evicting old entry by overwrite. "
                        "old tag: %#x poisoned: %i\n", block.tag, block.poisoned);

        ++rcacheStats.writeConflicts;
        if (block.poisoned)
            ++rcacheStats.writeCleanses;
    }

    block.tag = getTag(addr);
    block.valid = true;
    block.poisoned = false;
    ++rcacheStats.writes;

    LSQRequest *req = dynamic_cast<LSQRequest*>(pkt->senderState);
    if (req->isPoisoned()) {
        DPRINTF(RCache, "Write was poisoned. Poisoning cache block.\n");
        block.poisoned = true;
        ++rcacheStats.poisonedWrites;
    }

    uint8_t *pkt_data = block.data + pkt->getOffset(blockSize);
    pkt->writeDataToBlock(block.data, blockSize);
    // Write the written data back into the packet
    pkt->setData(pkt_data);
}

uint8_t*
RunaheadCache::read(PacketPtr pkt)
{
    assert(pkt->isRead());

    Addr addr = pkt->getAddr();
    DPRINTF(RCache, "Performing R-cache read of block %llu (addr %#x, unaligned %#x)\n",
            getIndex(addr), align(addr), addr);
    
    if (!lookup(addr)) {
        DPRINTF(RCache, "Tag lookup failed or block was invalid.\n");
        ++rcacheStats.readMisses;
        return nullptr;
    }

    ++rcacheStats.readHits;
    CacheBlock block = cacheEntries[getIndex(addr)];
    pkt->setDataFromBlock(block.data, blockSize);

    if (block.poisoned) {
        LSQRequest *req = dynamic_cast<LSQRequest*>(pkt->senderState);
        DPRINTF(RCache, "Cache block was poisoned, marking request as poisoned.\n");
        req->setPoisoned();
    }

    return block.data;
}

void
RunaheadCache::poisonblock(Addr addr)
{
    DPRINTF(RCache, "R-cache poisoning block %#x\n", align(addr));
    CacheBlock block = cacheEntries[getIndex(addr)];
    if (block.tag == getTag(addr))
        block.poisoned = true;

    ++rcacheStats.poisons;
}

void
RunaheadCache::invalidateCache()
{
    DPRINTF(RCache, "Invalidating (entire) r-cache.\n");
    for (CacheBlock &block : cacheEntries) {
        block.valid = false;
        block.poisoned = false;
    }

    ++rcacheStats.invalidations;
}

bool
RunaheadCache::handlePacket(PacketPtr pkt)
{
    DPRINTF(RCache, "R-cache received packet (addr %#x). Read: %i\n",
            pkt->getAddr(), pkt->isRead());

    ++rcacheStats.packetsHandled;

    bool success = true;
    if (pkt->isWrite()) {
        write(pkt);
    } else if (pkt->isRead()) {
        uint8_t *data = read(pkt);
        // Lookup failed
        if (data == nullptr)
            success = false;
    } else {
        panic("RE cache doesn't know what to do with packet of cmd type %s!!\n", pkt->cmdString());
    }

    if (success) {
        // Convert the packet into a response if needed
        if (pkt->needsResponse())
            pkt->makeResponse();
    }

    return success;
}

RunaheadCache::RCacheStats::RCacheStats(statistics::Group *parent)
    : statistics::Group(parent, "rcache"),
      ADD_STAT(lookups, statistics::units::Count::get(),
           "Total amount of cache block lookups"),
      ADD_STAT(writes, statistics::units::Count::get(),
           "Total amount of writes to R-cache"),
      ADD_STAT(writeConflicts, statistics::units::Count::get(),
           "Total amount of cache conflicts leading to eviction"),
      ADD_STAT(poisonedWrites, statistics::units::Count::get(),
           "Total amount of writes to R-cache containing poisoned data"),
      ADD_STAT(writeCleanses, statistics::units::Count::get(),
           "Total amount of cache cleanses caused by writing clean data into a poisoned block"),
      ADD_STAT(readMisses, statistics::units::Count::get(),
           "Total amount of cache misses on reads"),
      ADD_STAT(readHits, statistics::units::Count::get(),
           "Total amount of cache hits on reads"),
      ADD_STAT(poisons, statistics::units::Count::get(),
           "Total amount of times a cache block was poisoned"),
      ADD_STAT(invalidations, statistics::units::Count::get(),
           "Total amount of times the R-cache was invalidated"),
      ADD_STAT(packetsHandled, statistics::units::Count::get(),
           "Total amount of packets served by runahead cache")
{
    lookups.prereq(lookups);
    writes.prereq(writes);
    writeConflicts.prereq(writeConflicts);
    poisonedWrites.prereq(poisonedWrites);
    writeCleanses.prereq(writeCleanses);
    readMisses.prereq(readMisses);
    readHits.prereq(readHits);
    poisons.prereq(poisons);
    invalidations.prereq(invalidations);
    packetsHandled.prereq(packetsHandled);
}

} // namespace runahead
} // namespace gem5
