from cache.three_level_cache import ThreeLevelCacheHierarchy
from gem5.components.memory.multi_channel import DualChannelDDR3_1600


def add_memory_args(parser):
    mem_group = parser.add_argument_group(title='Memory parameters')

    mem_group.add_argument('--mem-size', default='2GB', help='The amount of primary memory to use. Default: 2GB')

    cache_group = parser.add_argument_group(title='Cache parameters')

    cache_group.add_argument_group(title='Cache parameters')

    cache_group.add_argument('--l1i-size', default='32KiB', help='Amount of L1I cache memory')
    cache_group.add_argument('--l1i-assoc', default=8, type=int, help='Associativity of the L1I cache')

    cache_group.add_argument('--l1d-size', default='32KiB', help='Amount of L1D cache memory')
    cache_group.add_argument('--l1d-assoc', default=8, type=int, help='Associativity of the L1D cache')

    cache_group.add_argument('--l2-size', default='256KiB', help='Amount of L2 cache memory')
    cache_group.add_argument('--l2-assoc', default=8, type=int, help='Associativity of the L2 cache')
    cache_group.add_argument('--l2-banks', default=1, type=int, help='The amount of L2 banks')

    cache_group.add_argument('--l3-size', default='6MB', help='Amount of L3 cache memory')
    cache_group.add_argument('--l3-assoc', default=12, help='Associativity of the L3 cache')

def setup_cache(args):
    print('Configuring cache hierarchy...')
    caches = ThreeLevelCacheHierarchy(
        l1i_size=args.l1i_size,
        l1i_assoc=args.l1i_assoc,
        l1d_size=args.l1d_size,
        l1d_assoc=args.l1d_assoc,
        l2_size=args.l2_size,
        l2_assoc=args.l2_assoc,
        l3_size=args.l3_size,
        l3_assoc=args.l3_assoc,
    )

    return caches


def setup_memory(args) -> tuple:
    print('Configuring DRAM...')
    memory = DualChannelDDR3_1600(size=args.mem_size)
    return memory
