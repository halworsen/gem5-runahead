def add_parser_args(parser):
    sim_group = parser.add_argument_group(title='Simulation parameters')

    sim_group.add_argument(
        '--max-insts',
        help='Max amount of instructions to simulate in the ROI.'
    )

    board_group = parser.add_argument_group(title='Board parameters')

    board_group.add_argument('--kernel', required=True, help='Linux kernel to use')
    board_group.add_argument('--image', required=True, help='Disk image to use')
    board_group.add_argument('--script', required=True, help='Runscript to execute after boot')

    board_group.add_argument('--clock', default='3.2GHz', help='Board clock frequency')
