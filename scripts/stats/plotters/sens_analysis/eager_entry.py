from ..plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np
import os
import re
import json
import logging

LOG = logging.getLogger(__name__)

class EagerEntryIPC(Plotter):
    name = 'Relative NIPC to baseline for runahead entry policies'
    fname = 'eager_entry'
    description = 'Sensitivity analysis of IPC improvements relative to baseline for eager runahead entry'

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
            lazy_path = os.path.join(
                self.log_dir, bench_dir.name,
                'm5out-gem5-spec2017-bench-traditional-re-ift-300-lazy-entry', 'gem5stats.json'
            )
            eager_path = os.path.join(
                self.log_dir, bench_dir.name,
                'm5out-gem5-spec2017-bench-traditional-re-ift-300-eager-entry', 'gem5stats.json'
            )

            # Read their stats
            LOG.debug(f'\t reading eager stats')
            with open(eager_path, 'r') as f:
                if not bench_dir.name in self.data:
                    self.data[bench_dir.name] = {}
                data = json.load(f)
                self.data[bench_dir.name]['eager'] = data['sections'][0]['stats']
            
            LOG.debug(f'\t reading lazy stats')
            with open(lazy_path, 'r') as f:
                if not bench_dir.name in self.data:
                    self.data[bench_dir.name] = {}
                data = json.load(f)
                self.data[bench_dir.name]['lazy'] = data['sections'][0]['stats']

    def plot(self) -> None:
        benchmarks = [b for b in self.data.keys()]
        benchmarks = sorted(benchmarks)
        runs = ['eager', 'lazy']

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

        plt.ylabel('Relative NIPC')
        plt.xticks(
            np.array(xs) + (bar_width * len(runs)) / 2,
            list(benchmarks) + ['gmean'],
            rotation=90
        )
        plt.ylim(0.9)

        plt.legend(
            ['Eager entry', 'Lazy entry'],
            ncol=2,
            markerscale=0.5,
            fontsize=8,
            handleheight=0.7,
            handlelength=0.7,
            loc='upper left'
        )
        leg = plt.gca().get_legend()
        for i, handle in enumerate(leg.legendHandles):
            handle.set_color(self.run_colors[i])

        plt.axhline(y=1.0, color='black', linewidth=0.5, linestyle='--')
