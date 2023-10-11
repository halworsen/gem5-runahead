from plotters.simpoint_weights import SimPointWeights

from argparse import ArgumentParser
from os import makedirs
import os.path
import json

import matplotlib.pyplot as plt

ALL_PLOTS = [
    SimPointWeights,
    # LoadToUse,
    # InterimInsts,
    # RECycles,
    # TriggerInFlightCycles,
]

parser = ArgumentParser()
parser.add_argument(
    '--out', metavar='plots_path',
    type=str,
    default='./gem5-stats-plots',
    help='directory to output the plots in. '
         'default: ./gem5-stat-plots',
)

if __name__ == '__main__':
    opts = parser.parse_args()

    if not os.path.exists(opts.out):
        makedirs(opts.out)

    for plotter in ALL_PLOTS:
        plotter = plotter()
        plotter.load_data()

        # Reset
        plt.figure()
        plt.title(plotter.name)

        # Plot
        plotter.plot()

        path = os.path.join(opts.out, f'{plotter.fname}.png')
        plt.tight_layout()
        plt.savefig(path, dpi=300)
