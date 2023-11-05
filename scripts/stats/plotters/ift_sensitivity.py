from .plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np
import os
import re
import json
import logging

LOG = logging.getLogger(__name__)

class IFTSensitivity(Plotter):
    name = 'Relative IPC to baseline for runahead IFTs'
    fname = 'ift_sensitivity'
    description = 'Sensitivity analysis of IPC improvements relative to baseline with various LLL in-flight cycle thresholds'

    log_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'

    run_pat = re.compile(r'm5out\-gem5\-spec2017\-bench\-traditional\-re\-ift\-(\d+)')

    run_colors = [
        'green',
        'red',
        'orange',
        'blue',
        'magenta',
    ]

    def load_data(self) -> None:
        self.data = {}
        for bench_dir in os.scandir(self.log_dir):
            if bench_dir.name not in self.valid_benchmarks:
                continue
            self.data[bench_dir.name] = {}
            LOG.info(f'reading data for benchmark: {bench_dir.name}')

            # Read the baseline stats
            baseline_stats_path = os.path.join(
                self.log_dir,
                bench_dir.name,
                'm5out-gem5-spec2017-bench-baseline-o3',
                'gem5stats.json'
            )
            LOG.info('\t reading baseline stats')
            with open(baseline_stats_path, 'r') as f:
                data = json.load(f)
                self.data[bench_dir.name]['baseline'] = data['sections'][0]['stats']

            # Find all the In-Flight Threshold simulation outputs
            for run_dir in os.scandir(bench_dir):
                match = self.run_pat.match(run_dir.name)
                if not match:
                    continue

                # Read their stats
                LOG.info(f'\t reading stats: {run_dir.name}')
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
            gmeans.append(np.sqrt(np.prod(ipcs)))
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

        tick_offsets = [(len(runs) * bar_width) / 2 for runs in self.data.values()]

        plt.ylabel('Relative IPC')
        plt.xticks(
            np.array(xs),
            list(benchmarks) + ['gmean'],
            rotation=90
        )
        plt.ylim(0, 1.5)

        plt.legend(
            runs,
            markerscale=0.5,
            fontsize=8,
            handleheight=0.7,
            handlelength=0.7,
            loc='upper right'
        )
        leg = plt.gca().get_legend()
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.run_colors[i])

        plt.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--')
