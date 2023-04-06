from gem5.components.cachehierarchies.classic.abstract_classic_cache_hierarchy import AbstractClassicCacheHierarchy
from gem5.components.cachehierarchies.abstract_cache_hierarchy import AbstractCacheHierarchy
from gem5.components.boards.abstract_board import AbstractBoard
from gem5.utils.override import overrides
from gem5.components.cachehierarchies.classic.caches.l1dcache import L1DCache
from gem5.components.cachehierarchies.classic.caches.l1icache import L1ICache
from gem5.components.cachehierarchies.classic.caches.l2cache import L2Cache
from gem5.components.cachehierarchies.classic.caches.mmu_cache import MMUCache
from gem5.isas import ISA
from m5.objects import Port, SystemXBar, Cache, BasePrefetcher, L2XBar, BadAddr

class L3Cache(Cache):
    def __init__(
        self,
        size: str = '6MB',
        assoc: int = 12,
        tag_latency: int = 30,
        data_latency: int = 30,
        response_latency: int = 1,
        mshrs: int = 20,
        tgts_per_mshr: int = 12,
        writeback_clean: bool = True,
        prefetcher: BasePrefetcher = None,
    ):
        super().__init__()

        self.size = size
        self.assoc = assoc

        self.tag_latency = tag_latency
        self.data_latency = data_latency
        self.response_latency = response_latency

        self.mshrs = mshrs
        self.tgts_per_mshr = tgts_per_mshr

        self.writeback_clean = writeback_clean

        if prefetcher is not None:
            self.prefetcher = prefetcher()

class ThreeLevelCacheHierarchy(
    AbstractClassicCacheHierarchy,
):
    def __init__(
        self,
        l1d_size: str,
        l1d_assoc: int,
        l1i_size: str,
        l1i_assoc: int,
        l2_size: str,
        l2_assoc: int,
        l3_size: str,
        l3_assoc: int,
    ):
        super().__init__()

        self._l1d_size = l1d_size
        self._l1d_assoc = l1d_assoc
        self._l1i_size = l1i_size
        self._l1i_assoc = l1i_assoc
        self._l2_size = l2_size
        self._l2_assoc = l2_assoc
        self._l3_size = l3_size
        self._l3_assoc = l3_assoc

        self.membus = SystemXBar(width=64)
        self.membus.badaddr_responder = BadAddr()
        self.membus.default = self.membus.badaddr_responder.pio

        # L2 gets a bus to interface with the L1 D- and I-caches
        self.l2_bus = L2XBar()
        # No L3 Xbar, L2 can directly interface with L3 since there is only 1 L2 cache

    def get_mem_side_port(self) -> Port:
        return self.membus.mem_side_ports

    def get_cpu_side_port(self) -> Port:
        return self.membus.cpu_side_ports

    @overrides(AbstractCacheHierarchy)
    def incorporate_cache(self, board: AbstractBoard):
        self.l1d_cache = L1DCache(self._l1d_size, self._l1d_assoc, tag_latency=2, data_latency=2)
        self.l1i_cache = L1ICache(self._l1i_size, self._l1i_assoc, tag_latency=2, data_latency=2)
        self.l2_cache = L2Cache(self._l2_size, self._l2_assoc, tag_latency=8, data_latency=8)
        self.l3_cache = L3Cache(self._l3_size, self._l3_assoc, tag_latency=30, data_latency=30)

        # ITLB Page walk caches
        self.iptw_cache = MMUCache(size='8KiB')
        # DTLB Page walk caches
        self.dptw_cache = MMUCache(size='8KiB')

        # Connect L1 caches
        cpu = board.get_processor().get_cores()[0]
        cpu.connect_dcache(self.l1d_cache.cpu_side)
        cpu.connect_icache(self.l1i_cache.cpu_side)
        cpu.connect_walker_ports(self.iptw_cache.cpu_side, self.dptw_cache.cpu_side)

        self.l1i_cache.mem_side = self.l2_bus.cpu_side_ports
        self.l1d_cache.mem_side = self.l2_bus.cpu_side_ports
        self.iptw_cache.mem_side = self.l2_bus.cpu_side_ports
        self.dptw_cache.mem_side = self.l2_bus.cpu_side_ports

        # Connect L2 caches
        self.l2_cache.cpu_side = self.l2_bus.mem_side_ports
        self.l2_cache.mem_side = self.l3_cache.cpu_side

        # Connect L3 caches
        #self.l3_cache.cpu_side = self.l2_cache.mem_side <- done by L2 cache connection
        self.l3_cache.mem_side = self.membus.cpu_side_ports

        # Connect the memory bus
        board.connect_system_port(self.membus.cpu_side_ports)
        for ctl in board.get_memory().get_memory_controllers():
            ctl.port = self.membus.mem_side_ports

        # Finally, connect interrupt ports
        if board.get_processor().get_isa() == ISA.X86:
            int_req_port = self.membus.mem_side_ports
            int_resp_port = self.membus.cpu_side_ports
            cpu.connect_interrupt(int_req_port, int_resp_port)
        else:
            cpu.connect_interrupt()

        # Coherent IO cache
        if board.has_coherent_io():
            self._setup_io_cache(board)

    def _setup_io_cache(self, board: AbstractBoard) -> None:
        """Create a cache for coherent I/O connections"""
        self.iocache = Cache(
            assoc=8,
            tag_latency=50,
            data_latency=50,
            response_latency=50,
            mshrs=20,
            size="1kB",
            tgts_per_mshr=12,
            addr_ranges=board.mem_ranges,
        )
        self.iocache.mem_side = self.membus.cpu_side_ports
        self.iocache.cpu_side = board.get_mem_side_coherent_io_port()
