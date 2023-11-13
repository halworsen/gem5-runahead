from ..plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np
import os
import re
import json
import logging

LOG = logging.getLogger(__name__)

class IFTSensitivity(Plotter):
    name = 'IPC relative to baseline for runahead IFTs'
    fname = 'ift_sensitivity'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    run_pat = re.compile(r'^m5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)$')

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            LOG.debug(f'reading data for benchmark: {bench}')

            # Read the baseline stats
            LOG.debug('\t reading baseline stats')
            self.data[bench]['baseline'] = self.read_stats(
                bench,
                'm5out-gem5-spec2017-bench-baseline-o3'
            )

            # Find all the In-Flight Threshold simulation outputs
            for run in self.experiments(bench):
                match = self.run_pat.match(run)
                if not match:
                    continue
                ift = match.group(1)

                # Read their stats
                LOG.debug(f'\t reading stats: {run}')
                self.data[bench][ift] = self.read_stats(bench, run)

    def plot(self) -> None:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = [run for run in self.data[list(self.data.keys())[0]].keys() if run != 'baseline']
        runs = sorted(runs, key=lambda r: int(r))

        # Compute relative IPC to the baseline benchmark
        relative_ipcs = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'computing relative IPCs for {bench}:')
            base_ipc = self.read_stat('system.processor.cores1.core.ipc', data=data['baseline'])
            for run in runs:
                ipc = self.read_stat('system.processor.cores1.core.ipc', data=data[run])
                relative_ipcs[bench].append(ipc / base_ipc)
                LOG.info(f'\t{run} - {relative_ipcs[bench][-1]}')

        # 2nd pass to compute geometric mean IPC increase across all benchmarks
        gmeans = []
        LOG.info('computing geometric mean relative IPCs')
        for i, run in enumerate(runs):
            ipcs = []
            # collect all relative IPCs for this run across all benchmarks
            for bench in relative_ipcs.keys():
                ipcs.append(relative_ipcs[bench][i])
            ipcs = np.array(ipcs)
            gmeans.append(np.power(np.prod(ipcs), 1/len(benchmarks)))
            LOG.info(f'\t{run} - {gmeans[-1]}')
        relative_ipcs['gmeans'] = gmeans

        # plot each benchmark's relative IPCs
        self.plot_grouped_bars(relative_ipcs, plt)

        plt.ylabel('Relative IPC')
        plt.ylim(0.8)
        plt.legend(
            runs,
            ncol=2,
            markerscale=0.5, fontsize=8,
            handleheight=0.7, handlelength=0.7,
            loc='lower left'
        )
        leg = plt.gca().get_legend()
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.color_pool[i])

        plt.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--', label='_nolegend_')

class IFTSensitivityReal(IFTSensitivity):
    name = 'NIPC relative to baseline for runahead IFTs'
    fname = 'ift_sensitivity_real'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    def plot(self) -> None:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = [run for run in self.data[list(self.data.keys())[0]].keys() if run != 'baseline']
        runs = sorted(runs, key=lambda r: int(r))

        fig, (ax1, ax2) = plt.subplots(ncols=2)

        # Compute relative IPC to the baseline benchmark
        relative_ipcs = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'computing relative IPCs for {bench}:')
            base_ipc = self.read_stat('system.processor.cores1.core.realIpc', data=data['baseline'])
            for run in runs:
                ipc = self.read_stat('system.processor.cores1.core.realIpc', data=data[run])
                relative_ipcs[bench].append(ipc / base_ipc)
                LOG.info(f'\t{run} - {relative_ipcs[bench][-1]}')

        # 2nd pass to compute geometric mean IPC increase across all benchmarks
        gmeans = []
        LOG.info('computing geometric mean relative IPCs')
        for i, run in enumerate(runs):
            ipcs = []
            # collect all relative IPCs for this run across all benchmarks
            for bench in relative_ipcs.keys():
                ipcs.append(relative_ipcs[bench][i])
            ipcs = np.array(ipcs)
            gmeans.append(np.power(np.prod(ipcs), 1/len(benchmarks)))
            LOG.info(f'\t{run} - {gmeans[-1]}')
        relative_ipcs['gmean'] = gmeans

        # plot each benchmark's relative IPCs
        self.plot_grouped_bars(relative_ipcs, ax1)
        ax1.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--', label='_nolegend_')

        ax1.set_ylabel('Relative NIPC')
        ax1.set_ylim(0.9)
        leg = fig.legend(
            runs,
            ncol=2,
            markerscale=0.5, fontsize=8,
            handleheight=0.7, handlelength=0.7,
            bbox_to_anchor=(0.19, 0.86)
        )
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.color_pool[i])

        # Read pseudoretired insts
        pseudoretireds = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'reading pseudoretired insts for {bench}:')
            for run in runs:
                pr = self.read_stat('system.processor.cores1.core.pseudoRetiredInsts', data=data[run])
                pseudoretireds[bench].append(pr)
                LOG.info(f'\t{run} - {pseudoretireds[bench][-1]}')

        # 2nd pass to compute means
        means = []
        LOG.info('computing means')
        for i, run in enumerate(runs):
            prs = []
            # collect all relative prs for this run across all benchmarks
            for bench in pseudoretireds.keys():
                prs.append(pseudoretireds[bench][i])
            prs = np.array(prs)
            means.append(prs.mean())
            LOG.info(f'\t{run} - {means[-1]}')
        pseudoretireds['mean'] = means

        self.plot_grouped_bars(pseudoretireds, ax2)

        ax2.set_title('Insts pseudoretired')
        ax2.set_ylabel('Instructions')
        ax2.set_yscale('log')

        fig.set_figwidth(10)

class OverheadAdjustedIFTSensitivity(IFTSensitivity):
    name = 'Overhead-adjusted IPC relative to baseline for runahead IFTs'
    fname = 'ift_sensitivity_overhead_adjusted'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    def plot(self) -> None:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = [run for run in self.data[list(self.data.keys())[0]].keys() if run != 'baseline']
        runs = sorted(runs, key=lambda r: int(r))

        # Compute relative IPC to the baseline benchmark
        relative_ipcs = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'computing relative IPCs for {bench}:')
            base_ipc = self.read_stat('system.processor.cores1.core.ipc', data=data['baseline'])
            for run in runs:
                insts = self.read_stat('simInsts', data=data[run])
                adjusted_cycles = self.read_stat('system.processor.cores1.core.numCycles', data=data[run])
                adjusted_cycles -= self.read_stat('system.processor.cores1.core.commit.totalRunaheadOverhead', data=data[run])
                adjusted_ipc = insts / adjusted_cycles
                relative_ipcs[bench].append(adjusted_ipc / base_ipc)
                LOG.info(f'\t{run} - {relative_ipcs[bench][-1]}')

        # 2nd pass to compute geometric mean IPC increase across all benchmarks
        gmeans = []
        LOG.info('computing geometric mean relative IPCs')
        for i, run in enumerate(runs):
            ipcs = []
            # collect all relative IPCs for this run across all benchmarks
            for bench in relative_ipcs.keys():
                ipcs.append(relative_ipcs[bench][i])
            ipcs = np.array(ipcs)
            gmeans.append(np.power(np.prod(ipcs), 1/len(benchmarks)))
            LOG.info(f'\t{run} - {gmeans[-1]}')
        relative_ipcs['gmean'] = gmeans

        self.plot_grouped_bars(relative_ipcs, plt)
        plt.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--', label='_nolegend_')

        # labels, ticks, etc.
        plt.ylabel('Relative IPC')
        plt.ylim(0.9)

        plt.legend(
            runs,
            ncol=2,
            markerscale=0.5, fontsize=8,
            handleheight=0.7, handlelength=0.7,
            loc='lower left'
        )
        leg = plt.gca().get_legend()
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.color_pool[i])
