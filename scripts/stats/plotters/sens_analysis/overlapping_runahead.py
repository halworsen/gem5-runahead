from ..plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np
import os
import re
import json
import logging

LOG = logging.getLogger(__name__)

class OverlappingRE(Plotter):
    name = 'Sensitivity analysis of overlapping runahead'
    fname = 'overlapping_re'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    log_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'

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
                self.log_dir, bench_dir.name,
                'm5out-gem5-spec2017-bench-baseline-o3', 'gem5stats.json'
            )
            LOG.debug('\t reading baseline stats')
            with open(baseline_stats_path, 'r') as f:
                data = json.load(f)
                self.data[bench_dir.name]['baseline'] = data['sections'][0]['stats']

            # Read the simulation outputs
            no_overlap_path = os.path.join(
                self.log_dir, bench_dir.name,
                'm5out-gem5-spec2017-bench-traditional-re-ift-300-no-overlap', 'gem5stats.json'
            )
            overlap_path = os.path.join(
                self.log_dir, bench_dir.name,
                'm5out-gem5-spec2017-bench-traditional-re-ift-300-overlap', 'gem5stats.json'
            )

            # Read their stats
            LOG.debug(f'\t reading no overlap stats')
            with open(no_overlap_path, 'r') as f:
                if not bench_dir.name in self.data:
                    self.data[bench_dir.name] = {}
                data = json.load(f)
                self.data[bench_dir.name]['no_overlap'] = data['sections'][0]['stats']

            LOG.debug(f'\t reading overlap stats')
            with open(overlap_path, 'r') as f:
                if not bench_dir.name in self.data:
                    self.data[bench_dir.name] = {}
                data = json.load(f)
                self.data[bench_dir.name]['overlap'] = data['sections'][0]['stats']

    def plot(self) -> None:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = ['overlap', 'no_overlap']

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

        ax1.set_title('Relative NIPCs')
        ax1.set_ylabel('Relative NIPC')
        ax1.set_xticks(
            np.array(xs) + (bar_width * len(runs)) / 2,
            list(benchmarks) + ['gmean'],
            rotation=90
        )
        ax1.set_ylim(0.9)

        leg = fig.legend(
            ['Overlap allowed', 'Overlap not allowed'],
            ncol=2,
            markerscale=0.5,
            fontsize=8,
            handleheight=0.7,
            handlelength=0.7,
            bbox_to_anchor=(0.65, 0.93)
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
