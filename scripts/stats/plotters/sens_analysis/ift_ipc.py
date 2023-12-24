from ..plotter import Plotter
from ..frame_util import FrameConstructor, FrameMeans
import matplotlib.pyplot as plt
import numpy as np
import re
import logging
from pandas import DataFrame
import seaborn as sbn
import scipy

LOG = logging.getLogger(__name__)

class IFTIPC(Plotter):
    name = 'Sensitivity analysis of runahead IFTs'
    fname = 'baseline_sensitivity/ift_sensitivity'
    description = 'Sensitivity analysis of NIPCs and pseudoretired insts relative to baseline with various LLL in-flight cycle thresholds'

    def __init__(self, m5out_pattern: str) -> None:
        self.run_pat = re.compile(m5out_pattern)

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            LOG.debug(f'reading data for benchmark: {bench}')

            # Read the baseline stats
            LOG.debug('\t reading baseline stats')
            self.data[bench]['baseline'] = self.read_stats(bench, 'm5out-spec2017-o3-baseline')

            # Find all the In-Flight Threshold simulation outputs
            for run in self.experiments(bench):
                match = self.run_pat.match(run)
                if not match:
                    continue
                ift = match.group(1)

                # Read their stats
                LOG.debug(f'\t reading stats: {run}')
                self.data[bench][ift] = self.read_stats(bench, run)

    def construct_ipc_frame(self) -> None:
        '''
        Construct the frame with relative NIPCs
        '''
        frame = FrameConstructor.relative_frame(
            self.data,
            'system.processor.cores1.core.realIpc',
            relative_to='baseline',
        )
        frame.sort_values(by='benchmark', ascending=True, inplace=True)
        FrameMeans.add_hmean(frame)

        self.ipc_frame = frame
    
    def construct_retired_frame(self) -> None:
        '''
        Construct the frame with pseudoretired instruction counts
        '''
        frame = FrameConstructor.value_frame(
            self.data,
            'system.processor.cores1.core.pseudoRetiredInsts',
            exclude='baseline',
        )
        frame.sort_values(by='benchmark', ascending=True, inplace=True)
        FrameMeans.add_mean(frame)

        self.pseudoretired_frame = frame

    def construct_frames(self) -> None:
        self.construct_ipc_frame()
        self.construct_retired_frame()

    def plot(self) -> None:
        fig = plt.figure(figsize=(16,5))
        gs = fig.add_gridspec(1, 2)

        sbn.set_style('whitegrid')
        ax = fig.add_subplot(gs[0, 0])
        ipc_plot = sbn.barplot(
            data=self.ipc_frame,
            x='benchmark',
            y='value',
            hue='experiment',
            order=self.ipc_frame['benchmark']
        )
        ipc_plot.bar_label(ipc_plot.containers[5], fontsize=6, rotation=90, fmt='%.4f')
        ipc_plot.set_title('NIPC relative to stock CPU')
        for l in ipc_plot.get_xticklabels():
            l.set_rotation(90)
        ipc_plot.set_xlabel('')
        ipc_plot.set_ylabel('Relative NIPC')
        ipc_plot.set_ylim(0.9)

        ax = fig.add_subplot(gs[0, 1])
        pr_plot = sbn.barplot(
            data=self.pseudoretired_frame,
            x='benchmark',
            y='value',
            hue='experiment',
            order=self.pseudoretired_frame['benchmark'],
            legend=False,
        )
        pr_plot.set_title('Instructions pseudoretired')
        for l in pr_plot.get_xticklabels():
            l.set_rotation(90)
        pr_plot.set_xlabel('')
        pr_plot.set_yscale('log')
        pr_plot.set_ylabel('Instructions')

        ipc_plot.legend(title='IFT', loc='upper left')
