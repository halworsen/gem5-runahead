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

    log_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'

    run_pat = re.compile(r'^m5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)$')

    run_colors = [
        'green',
        'red',
        'darkorange',
        'blue',
        'magenta',
        'darkslategray',
        'cyan',
        'tan',
    ]

    def load_data(self) -> None:
        self.data = {}
        for bench_dir in os.scandir(self.log_dir):
            if bench_dir.name not in self.valid_benchmarks:
                continue
            self.data[bench_dir.name] = {}
            LOG.debug(f'reading data for benchmark: {bench_dir.name}')

            # Read the baseline stats
            baseline_stats_path = os.path.join(
                self.log_dir,
                bench_dir.name,
                'm5out-gem5-spec2017-bench-baseline-o3',
                'gem5stats.json'
            )
            LOG.debug('\t reading baseline stats')
            with open(baseline_stats_path, 'r') as f:
                data = json.load(f)
                self.data[bench_dir.name]['baseline'] = data['sections'][0]['stats']

            # Find all the In-Flight Threshold simulation outputs
            for run_dir in os.scandir(bench_dir):
                match = self.run_pat.match(run_dir.name)
                if not match:
                    continue

                # Read their stats
                LOG.debug(f'\t reading stats: {run_dir.name}')
                stats_path = os.path.join(bench_dir.path, run_dir.name, 'gem5stats.json')
                with open(stats_path, 'r') as f:
                    if not bench_dir.name in self.data:
                        self.data[bench_dir.name] = {}
                    data = json.load(f)
                    self.data[bench_dir.name][match.group(1)] = data['sections'][0]['stats']

    def plot(self) -> None:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = [run for run in self.data[list(self.data.keys())[0]].keys() if run != 'baseline']
        runs = sorted(runs, key=lambda r: int(r))

        # Compute relative IPC to the baseline benchmark
        relative_ipcs = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'computing relative IPCs for {bench}:')
            base_ipc = data['baseline']['system']['processor']['cores1']['core']['ipc']['values'][0]
            for run in runs:
                ipc = data[run]['system']['processor']['cores1']['core']['ipc']['values'][0]
                relative_ipcs[bench].append(ipc / base_ipc)
                LOG.info(f'\t{run} - {relative_ipcs[bench][-1]}')

        # 2nd pass to compute geometric mean IPC increase across all benchmarks
        gmeans = []
        # runs = ('50', '100', '150', '200', '250')
        LOG.info('computing geometric mean relative IPCs')
        for i, run in enumerate(runs):
            ipcs = []
            # collect all relative IPCs for this run across all benchmarks
            for bench in relative_ipcs.keys():
                ipcs.append(relative_ipcs[bench][i])
            ipcs = np.array(ipcs)
            gmeans.append(np.power(np.prod(ipcs), 1/len(benchmarks)))
            LOG.info(f'\t{run} - {gmeans[-1]}')

        # plot each benchmark's relative IPCs
        xs = [0]
        bar_width = 1
        offsets = np.array([j * bar_width for j in range(len(runs))])
        assert(len(offsets) == len(runs))
        for bench in benchmarks:
            plt.bar(
                x=xs[-1] + offsets,
                height=np.array(relative_ipcs[bench]),
                width=bar_width,
                color=self.run_colors,
            )

            # add some bar widths of padding between each group
            new_x = xs[-1] + (bar_width * len(runs)) + 5 * bar_width
            xs.append(new_x)

        # plot geometric means
        plt.bar(
            x=xs[-1] + offsets,
            height=np.array(gmeans),
            width=bar_width,
            color=self.run_colors,
        )

        plt.ylabel('Relative IPC')
        plt.xticks(
            np.array(xs) + (bar_width * len(runs)) / 2,
            list(benchmarks) + ['gmean'],
            rotation=90
        )
        plt.ylim(0.8)

        plt.legend(
            runs,
            ncol=2,
            markerscale=0.5,
            fontsize=8,
            handleheight=0.7,
            handlelength=0.7,
            loc='lower left'
        )
        leg = plt.gca().get_legend()
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.run_colors[i])

        plt.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--')

class IFTSensitivityReal(Plotter):
    name = 'NIPC relative to baseline for runahead IFTs'
    fname = 'ift_sensitivity_real'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    log_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'

    run_pat = re.compile(r'^m5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)$')

    run_colors = [
        'green',
        'red',
        'darkorange',
        'blue',
        'magenta',
        'darkslategray',
        'cyan',
        'tan',
    ]

    def load_data(self) -> None:
        self.data = {}
        for bench_dir in os.scandir(self.log_dir):
            if bench_dir.name not in self.valid_benchmarks:
                continue
            self.data[bench_dir.name] = {}
            LOG.debug(f'reading data for benchmark: {bench_dir.name}')

            # Read the baseline stats
            baseline_stats_path = os.path.join(
                self.log_dir,
                bench_dir.name,
                'm5out-gem5-spec2017-bench-baseline-o3',
                'gem5stats.json'
            )
            LOG.debug('\t reading baseline stats')
            with open(baseline_stats_path, 'r') as f:
                data = json.load(f)
                self.data[bench_dir.name]['baseline'] = data['sections'][0]['stats']

            # Find all the In-Flight Threshold simulation outputs
            for run_dir in os.scandir(bench_dir):
                match = self.run_pat.match(run_dir.name)
                if not match:
                    continue

                # Read their stats
                LOG.debug(f'\t reading stats: {run_dir.name}')
                stats_path = os.path.join(bench_dir.path, run_dir.name, 'gem5stats.json')
                with open(stats_path, 'r') as f:
                    if not bench_dir.name in self.data:
                        self.data[bench_dir.name] = {}
                    data = json.load(f)
                    self.data[bench_dir.name][match.group(1)] = data['sections'][0]['stats']

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
            base_ipc = data['baseline']['system']['processor']['cores1']['core']['realIpc']['values'][0]
            for run in runs:
                ipc = data[run]['system']['processor']['cores1']['core']['realIpc']['values'][0]
                relative_ipcs[bench].append(ipc / base_ipc)
                LOG.info(f'\t{run} - {relative_ipcs[bench][-1]}')

        # 2nd pass to compute geometric mean IPC increase across all benchmarks
        gmeans = []
        # runs = ('50', '100', '150', '200', '250')
        LOG.info('computing geometric mean relative IPCs')
        for i, run in enumerate(runs):
            ipcs = []
            # collect all relative IPCs for this run across all benchmarks
            for bench in relative_ipcs.keys():
                ipcs.append(relative_ipcs[bench][i])
            ipcs = np.array(ipcs)
            gmeans.append(np.power(np.prod(ipcs), 1/len(benchmarks)))
            LOG.info(f'\t{run} - {gmeans[-1]}')

        # plot each benchmark's relative IPCs
        xs = [0]
        bar_width = 1
        offsets = np.array([j * bar_width for j in range(len(runs))])
        assert(len(offsets) == len(runs))
        for bench in benchmarks:
            ax1.bar(
                x=xs[-1] + offsets,
                height=np.array(relative_ipcs[bench]),
                width=bar_width,
                color=self.run_colors,
            )

            # add some bar widths of padding between each group
            new_x = xs[-1] + (bar_width * len(runs)) + 5 * bar_width
            xs.append(new_x)

        # plot geometric means
        ax1.bar(
            x=xs[-1] + offsets,
            height=np.array(gmeans),
            width=bar_width,
            color=self.run_colors,
        )

        ax1.set_ylabel('Relative NIPC')
        ax1.set_xticks(
            np.array(xs) + (bar_width * len(runs)) / 2,
            list(benchmarks) + ['gmean'],
            rotation=90
        )
        ax1.set_ylim(0.9)

        leg = fig.legend(
            runs,
            ncol=2,
            markerscale=0.5,
            fontsize=8,
            handleheight=0.7,
            handlelength=0.7,
            bbox_to_anchor=(0.19, 0.86)
        )
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.run_colors[i])

        ax1.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--')

        # Read pseudoretired insts
        pseudoretireds = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'reading pseudoretired insts for {bench}:')
            for run in runs:
                pr = data[run]['system']['processor']['cores1']['core']['pseudoRetiredInsts']['values'][0]
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

        xs = [0]
        bar_width = 1
        offsets = np.array([j * bar_width for j in range(len(runs))])
        assert(len(offsets) == len(runs))
        for bench in benchmarks:
            ax2.bar(
                x=xs[-1] + offsets,
                height=np.array(pseudoretireds[bench]),
                width=bar_width,
                color=self.run_colors,
            )

            # add some bar widths of padding between each group
            new_x = xs[-1] + (bar_width * len(runs)) + 5 * bar_width
            xs.append(new_x)

        # plot means
        ax2.bar(
            x=xs[-1] + offsets,
            height=np.array(means),
            width=bar_width,
            color=self.run_colors,
        )

        ax2.set_title('Insts pseudoretired')
        ax2.set_ylabel('Instructions')
        ax2.set_xticks(
            np.array(xs) + (bar_width * len(runs)) / 2,
            list(benchmarks) + ['mean'],
            rotation=90
        )
        ax2.set_yscale('log')

        fig.set_figwidth(10)

class OverheadAdjustedIFTSensitivity(Plotter):
    name = 'Overhead-adjusted IPC relative to baseline for runahead IFTs'
    fname = 'ift_sensitivity_overhead_adjusted'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    log_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'

    run_pat = re.compile(r'^m5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)$')

    run_colors = [
        'green',
        'red',
        'darkorange',
        'blue',
        'magenta',
        'darkslategray',
        'cyan',
        'tan',
    ]

    def load_data(self) -> None:
        self.data = {}
        for bench_dir in os.scandir(self.log_dir):
            if bench_dir.name not in self.valid_benchmarks:
                continue
            self.data[bench_dir.name] = {}
            LOG.debug(f'reading data for benchmark: {bench_dir.name}')

            # Read the baseline stats
            baseline_stats_path = os.path.join(
                self.log_dir,
                bench_dir.name,
                'm5out-gem5-spec2017-bench-baseline-o3',
                'gem5stats.json'
            )
            LOG.debug('\t reading baseline stats')
            with open(baseline_stats_path, 'r') as f:
                data = json.load(f)
                self.data[bench_dir.name]['baseline'] = data['sections'][0]['stats']

            # Find all the In-Flight Threshold simulation outputs
            for run_dir in os.scandir(bench_dir):
                match = self.run_pat.match(run_dir.name)
                if not match:
                    continue

                # Read their stats
                LOG.debug(f'\t reading stats: {run_dir.name}')
                stats_path = os.path.join(bench_dir.path, run_dir.name, 'gem5stats.json')
                with open(stats_path, 'r') as f:
                    if not bench_dir.name in self.data:
                        self.data[bench_dir.name] = {}
                    data = json.load(f)
                    self.data[bench_dir.name][match.group(1)] = data['sections'][0]['stats']

    def plot(self) -> None:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = [run for run in self.data[list(self.data.keys())[0]].keys() if run != 'baseline']
        runs = sorted(runs, key=lambda r: int(r))

        # Compute relative IPC to the baseline benchmark
        relative_ipcs = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'computing relative IPCs for {bench}:')
            base_ipc = data['baseline']['system']['processor']['cores1']['core']['ipc']['values'][0]
            for run in runs:
                insts = data[run]['simInsts']['values'][0]
                adjusted_cycles = data[run]['system']['processor']['cores1']['core']['numCycles']['values'][0]
                adjusted_cycles -= data[run]['system']['processor']['cores1']['core']['commit']['totalRunaheadOverhead']['values'][0]
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

        # plot each benchmark's relative IPCs
        xs = [0]
        bar_width = 1
        offsets = np.array([j * bar_width for j in range(len(runs))])
        assert(len(offsets) == len(runs))
        for bench in benchmarks:
            plt.bar(
                x=xs[-1] + offsets,
                height=np.array(relative_ipcs[bench]),
                width=bar_width,
                color=self.run_colors,
            )

            # add some bar widths of padding between each group
            new_x = xs[-1] + (bar_width * len(runs)) + 5 * bar_width
            xs.append(new_x)

        # plot geometric means
        plt.bar(
            x=xs[-1] + offsets,
            height=np.array(gmeans),
            width=bar_width,
            color=self.run_colors,
        )

        plt.ylabel('Relative IPC')
        plt.xticks(
            np.array(xs) + (bar_width * len(runs)) / 2,
            list(benchmarks) + ['gmean'],
            rotation=90
        )
        plt.ylim(0.9)

        plt.legend(
            runs,
            ncol=2,
            markerscale=0.5,
            fontsize=8,
            handleheight=0.7,
            handlelength=0.7,
            loc='lower left'
        )
        leg = plt.gca().get_legend()
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.run_colors[i])

        plt.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--')
