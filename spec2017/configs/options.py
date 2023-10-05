def add_parser_args(parser):
    sim_group = parser.add_argument_group(title='Simulation parameters')

    sim_group.add_argument(
        '--max-insts',
        help='Max amount of instructions to simulate on the detailed core.'
    )

    sim_group.add_argument(
        '--checkpoint-dir',
        help='The directory in which to store check- and simpoints',
        type=str,
    )
    sim_group.add_argument(
        '--simpoint-interval',
        help='The interval at which to record simpoints. 0 to disable',
        default=0,
        type=int,
    )

    board_group = parser.add_argument_group(title='Board parameters')

    board_group.add_argument('--kernel', required=True, help='Linux kernel to use')
    board_group.add_argument('--image', required=True, help='Disk image to use')
    board_group.add_argument('--script', required=True, help='Runscript to execute after boot')

    board_group.add_argument('--clock', default='3.2GHz', help='Board clock frequency')
