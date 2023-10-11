def add_parser_args(parser):
    sim_group = parser.add_argument_group(title='Simulation parameters')

    sim_group.add_argument(
        '--max-insts',
        help='Max amount of instructions to simulate on the detailed core.',
        default=0,
        type=int,
    )

    sim_group.add_argument(
        '--simpoint-interval',
        help='Perform a simpoint analysis with the given interval. If taking simpoint checkpoints, '
        'defines the interval at which the simpoints were taken.',
        default=0,
        type=int,
    )
    sim_group.add_argument(
        '--simpoint-checkpoints',
        help='Use a simple CPU to take checkpoints at every simpoint defined by the given file',
        default='',
        type=str,
    )
    sim_group.add_argument(
        '--warmup-insts',
        help='If taking checkpoints, checkpoint N insts earlier to allow for warmup just before the checkpoint',
        default=0,
        type=int,
    )

    sim_group.add_argument(
        '--restore-checkpoint',
        help='Restore simulation state using the given checkpoint',
        default=None,
        type=str,
    )

    board_group = parser.add_argument_group(title='Board parameters')

    board_group.add_argument('--kernel', required=True, help='Linux kernel to use')
    board_group.add_argument('--image', required=True, help='Disk image to use')
    board_group.add_argument('--script', required=True, help='Runscript to execute after boot')

    board_group.add_argument('--clock', default='3.2GHz', help='Board clock frequency')
