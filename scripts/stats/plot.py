from plotters.simpoint_weights import SimPointWeights
from plotters.sens_analysis.ift_sensitivity import IFTSensitivity, IFTSensitivityReal, OverheadAdjustedIFTSensitivity
from plotters.sens_analysis.l2u_overflows import L2UOverflows
from plotters.sens_analysis.commit_squash_cycles import CommitSquashCycles
from plotters.sens_analysis.mean_l2u import MeanL2U
from plotters.sens_analysis.overlapping_runahead import OverlappingRE
from plotters.sens_analysis.eager_entry import EagerEntryIPC
from plotters.sens_analysis.runahead_periods import IFTSensitivityREPeriods

from argparse import ArgumentParser
from os import makedirs
import os.path
import logging
import json

import matplotlib.pyplot as plt

ALL_PLOTS = [
    # Simpoints
    # SimPointWeights,

    # Sensitivity analysis
    # IFTSensitivity,
    # IFTSensitivityReal,
    OverheadAdjustedIFTSensitivity,
    # L2UOverflows,
    # CommitSquashCycles,
    # MeanL2U,
    # OverlappingRE,
    # EagerEntryIPC,
    # IFTSensitivityREPeriods,

    # Other
    # LoadToUse,
    # InterimInsts,
    # RECycles,
    # TriggerInFlightCycles,
]

logging.basicConfig(
    format='[%(levelname)s] %(name)s: %(message)s',
    level=logging.INFO,
)

parser = ArgumentParser()
parser.add_argument(
    '--out', metavar='plots_path',
    type=str,
    default='./gem5-stats-plots',
    help='directory to output the plots in. default: ./gem5-stat-plots',
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

        # Plot
        plotter.plot()

        plt.suptitle(plotter.name)

        path = os.path.join(opts.out, f'{plotter.fname}.png')
        plt.tight_layout()
        plt.savefig(path, dpi=300)
