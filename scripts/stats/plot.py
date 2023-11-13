from plotters.simpoint_weights import SimPointWeights

from plotters.sens_analysis.ift_ipc import IFTIPCReal
from plotters.sens_analysis.overlapping_runahead import OverlappingRE
from plotters.sens_analysis.eager_entry import EagerEntry

from plotters.exit_policy.interim_retired import InterimRetiredInsts

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
    IFTIPCReal(r'^m5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)$'),
    OverlappingRE(),
    EagerEntry(),

    # Debug/troubleshooting plots
    # IFTIPC(r'^m5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)$'),
    # OverheadAdjustedIFTIPC(r'^m5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)$'),
    # L2UOverflows,
    # CommitSquashCycles,
    # MeanL2U,
    # IFTIPCREPeriods,
    # IFTFracRunahead,
    # IFTBranchMispredicts,

    # Investigation of exit policy
    # InterimRetiredInsts,

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
    LOG = logging.getLogger(__name__)

    if not os.path.exists(opts.out):
        makedirs(opts.out)

    for plotter in ALL_PLOTS:
        LOG.info(f'Plotting {plotter.__class__.__name__} - {plotter.name}')
        plotter.load_data()
        plotter.construct_frames()

        if not plotter.manual:
            # Reset
            plt.figure()
            # Plot
            plotter.plot()

            # Set super title and save the plot
            plt.suptitle(plotter.name)
            path = os.path.join(opts.out, f'{plotter.fname}.png')
            plt.tight_layout()
            plt.savefig(path, dpi=300)
        else:
            plotter.plot()
