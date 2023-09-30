from m5.objects import Cache, BaseCPU, BaseXBar


class L1Cache(Cache):
    '''
    2-way associative L1 cache.
    '''
    assoc = 2
    tag_latency = 4
    data_latency = 4
    response_latency = 4
    mshrs = 4
    tgts_per_mshr = 20

    def __init__(self, size) -> None:
        super().__init__()
        self.size = size

    def _connCPU(self) -> None:
        pass

    def connect(self, component) -> None:
        if isinstance(component, BaseCPU):
            # connect to the CPU's cache
            self._connCPU(component)
        elif isinstance(component, BaseXBar):
            # from L1's POV, the (L2) crossbar is memory facing
            # from the memory's POV, L1 is CPU facing
            self.mem_side = component.cpu_side_ports
        else:
            raise ValueError(f'invalid component of type {type(component)}')


class L1ICache(L1Cache):
    is_read_only = True

    def _connCPU(self, component) -> None:
        self.cpu_side = component.icache_port


class L1DCache(L1Cache):
    def _connCPU(self, component) -> None:
        self.cpu_side = component.dcache_port


class L2Cache(Cache):
    '''
    8-way associative shared L2 cache.
    '''
    assoc = 8
    tag_latency = 20
    data_latency = 20
    response_latency = 20
    mshrs = 20
    tgts_per_mshr = 12

    def __init__(self, size) -> None:
        super().__init__()
        self.size = size

    def connect(self, bus, side='mem') -> None:
        '''
        The L2 cache may sit between two memory buses so the side param is required.
        '''
        if side not in ('cpu', 'mem'):
            raise ValueError(f'invalid connection side "{side}"')

        if side == 'cpu':
            port = bus.mem_side if isinstance(bus, Cache) else bus.mem_side_ports
            self.cpu_side = port
        elif side == 'mem':
            port = bus.cpu_side if isinstance(bus, Cache) else bus.cpu_side_ports
            self.mem_side = port


class L3Cache(Cache):
    '''
    12-way associative shared L3 cache
    '''
    assoc = 12
    tag_latency = 30
    data_latency = 30
    response_latency = 30
    mshrs = 30
    tgts_per_mshr = 10

    def __init__(self, size) -> None:
        super().__init__()
        self.size = size

    def connect(self, bus, side='mem') -> None:
        '''
        The L3 cache sits between two memory buses so the side param is required.
        '''
        if side not in ('cpu', 'mem'):
            raise ValueError(f'invalid connection side "{side}"')

        if side == 'cpu':
            port = bus.mem_side if isinstance(bus, Cache) else bus.mem_side_ports
            self.cpu_side = port
        elif side == 'mem':
            port = bus.cpu_side if isinstance(bus, Cache) else bus.cpu_side_ports
            self.mem_side = port
