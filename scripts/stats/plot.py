from plotters.simpoint_weights import SimPointWeights

from plotters.sens_analysis.ift_ipc import IFTIPCReal
from plotters.sens_analysis.overlapping_runahead import OverlappingRE
from plotters.sens_analysis.eager_entry import EagerEntry

from plotters.baseline_characteristics.interim_periods import BaselineInterimPeriods
from plotters.baseline_characteristics.ipc import BaselineIPC, BaselineAdjustedIPC
from plotters.baseline_characteristics.l2u import BaselineL2U
from plotters.baseline_characteristics.overhead import BaselineRunaheadOverhead
from plotters.baseline_characteristics.runahead_time import BaselineRunaheadTime, BaselineRunaheadFraction, BaselineRunaheadCycleFraction

from plotters.min_work.ipc import MinimumWorkSensitivityIPC, MinimumWorkIPC
from plotters.min_work.interim_periods import MinWorkInterimPeriods
from plotters.min_work.runahead_time import MinWorkRunaheadTime, MinWorkRunaheadFraction, MinWorkRunaheadCycleFraction, MinWorkRunaheadPeriods
from plotters.min_work.l2u import MinWorkL2U

from argparse import ArgumentParser
from os import makedirs
import os.path
import logging

import matplotlib.pyplot as plt

ALL_PLOTS = [
    # Simpoints
    # SimPointWeights(),

    # Sensitivity analysis
    # IFTIPCReal(r'^m5out\-spec2017\-traditional\-re\-ift\-(\d+)$'),
    # OverlappingRE(),
    # EagerEntry(),

    # Runahead baseline model characteristics
    # BaselineInterimPeriods(),
    # BaselineIPC(),
    # BaselineAdjustedIPC(),
    # BaselineL2U(),
    # BaselineRunaheadOverhead(),
    # BaselineRunaheadTime(),
    # BaselineRunaheadFraction(),
    # BaselineRunaheadCycleFraction(),

    # Minimum work model
    # MinimumWorkSensitivityIPC(
    #     r'^m5out\-spec2017\-re\-minwork-(deadline)\-(\d+)$',
    #     r'^m5out\-spec2017\-re\-minwork-(work)\-(\d+)$',
    # ),
    # MinimumWorkIPC(),
    # MinWorkInterimPeriods(),
    # MinWorkRunaheadTime(),
    # MinWorkRunaheadFraction(),
    # MinWorkRunaheadCycleFraction(),
    # MinWorkRunaheadPeriods(),
    MinWorkL2U(),
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
            os.makedirs(os.path.dirname(path), exist_ok=True)
            plt.tight_layout()
            plt.savefig(path, dpi=300, bbox_inches='tight')
        else:
            plotter.plot()
