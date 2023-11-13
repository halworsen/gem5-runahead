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
    name = 'IPC relative to baseline for runahead IFTs'
    fname = 'ift_sensitivity'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    def __init__(self, m5out_pattern: str) -> None:
        self.run_pat = re.compile(m5out_pattern)

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            LOG.debug(f'reading data for benchmark: {bench}')

            # Read the baseline stats
            LOG.debug('\t reading baseline stats')
            self.data[bench]['baseline'] = self.read_stats(bench, 'm5out-gem5-spec2017-bench-baseline-o3')

            # Find all the In-Flight Threshold simulation outputs
            for run in self.experiments(bench):
                match = self.run_pat.match(run)
                if not match:
                    continue
                ift = match.group(1)

                # Read their stats
                LOG.debug(f'\t reading stats: {run}')
                self.data[bench][ift] = self.read_stats(bench, run)

    def get_sorted_runs(self) -> tuple:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = [run for run in self.data[list(self.data.keys())[0]].keys() if run != 'baseline']
        runs = sorted(runs, key=lambda r: int(r))

        return (benchmarks, runs)

    def construct_frames(self) -> None:
        _, runs = self.get_sorted_runs()
        frame = DataFrame({
            'benchmark': [],
            'ift': [],
            'value': [],
        })

        # Compute relative IPC to the baseline benchmark
        for bench, data in self.data.items():
            LOG.debug(f'computing relative IPCs for {bench}:')
            base_ipc = self.stat('system.processor.cores1.core.ipc', data=data['baseline'])
            for run in runs:
                ipc = self.stat('system.processor.cores1.core.ipc', data=data[run])

                row = {'benchmark': bench, 'ift': run, 'value': ipc / base_ipc}
                frame.loc[len(frame)] = row

                LOG.debug(f'\t{run} - {ipc}')

        # sort dataframe by benchmark name
        frame.sort_values(by='benchmark', ascending=True, inplace=True)

        # 2nd pass to compute harmonic mean IPC increase across all benchmarks
        LOG.debug('computing harmonic mean relative IPCs')
        for run in runs:
            bmk_rows = frame[frame['benchmark'] != 'hmean']
            ipcs = bmk_rows[bmk_rows['ift'] == run]['value']
            ipcs = np.array(ipcs)
            hmean = scipy.stats.hmean(ipcs)

            row = {'benchmark': 'hmean', 'ift': run, 'value': hmean}
            frame.loc[len(frame)] = row

            LOG.debug(f'\t{run} - {hmean}')

        self.frame = frame

    def plot(self) -> None:
        plt.figure(figsize=(9, 5))

        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            data=self.frame,
            x='benchmark',
            y='value',
            hue='ift',
            order=self.frame['benchmark']
        )
        plot.bar_label(plot.containers[0], fontsize=6, rotation=90, fmt='%.4f')
        plot.bar_label(plot.containers[4], fontsize=6, rotation=90, fmt='%.4f')

        plt.ylim(0.9)
        plt.xticks(rotation=90)
        plt.xlabel('')
        plt.ylabel('Relative IPC')
        plt.legend(title='IFT', loc='upper left', bbox_to_anchor=(1,1))


class OverheadAdjustedIFTIPC(IFTIPCReal):
    name = 'Overhead-adjusted IPC relative to baseline for runahead IFTs'
    fname = 'ift_sensitivity_overhead_adjusted'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    def construct_frames(self) -> None:
        _, runs = self.get_sorted_runs()
        frame = DataFrame({
            'benchmark': [],
            'ift': [],
            'value': [],
        })

        # Compute adjusted IPC relative to the baseline benchmark
        for bench, data in self.data.items():
            LOG.debug(f'computing relative adjusted IPCs for {bench}:')
            base_ipc = self.stat('system.processor.cores1.core.realIpc', data=data['baseline'])
            for run in runs:
                insts = self.stat('simInsts', data=data[run])
                adjusted_cycles = self.stat('system.processor.cores1.core.numCycles', data=data[run])
                adjusted_cycles -= self.stat('system.processor.cores1.core.commit.totalRunaheadOverhead', data=data[run])
                adjusted_ipc = insts / adjusted_cycles

                relative_ipc = adjusted_ipc / base_ipc
                row = {'benchmark': bench, 'ift': run, 'value': relative_ipc}
                frame.loc[len(frame)] = row
                LOG.debug(f'\t{run} - {relative_ipc}')

        # sort dataframe by benchmark name
        frame.sort_values(by='benchmark', ascending=True, inplace=True)

        # 2nd pass to compute harmonic mean IPC increase across all benchmarks
        LOG.debug('computing harmonic mean relative IPCs')
        for run in runs:
            bmk_rows = frame[frame['benchmark'] != 'hmean']
            ipcs = bmk_rows[bmk_rows['ift'] == run]['value']
            ipcs = np.array(ipcs)
            hmean = scipy.stats.hmean(ipcs)

            row = {'benchmark': 'hmean', 'ift': run, 'value': hmean}
            frame.loc[len(frame)] = row

            LOG.debug(f'\t{run} - {hmean}')

        self.frame = frame

    def plot(self) -> None:
        plt.figure(figsize=(9, 5))

        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            data=self.frame,
            x='benchmark',
            y='value',
            hue='ift',
        )
        plot.bar_label(plot.containers[4], fontsize=6, rotation=90, fmt='%.4f')

        plt.xticks(rotation=90)
        plt.xlabel('')
        plt.ylabel('Relative IPC')
        plt.ylim(0.9)
        plt.legend(title='IFT', loc='upper left', bbox_to_anchor=(1,1))
