from ..plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np
import os
import re
import json
import logging

LOG = logging.getLogger(__name__)

class IFTFracRunahead(Plotter):
    name = 'Fraction of retired instructions that are runahead for different IFTs'
    fname = 'ift_frac_runahead'
    description = 'Fraction of total insts that were runahead'

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
        re_fracs = {b: [] for b in benchmarks}
        for bench, data in self.data.items():
            LOG.info(f'computing RE fractions for {bench}:')
            for run in runs:
                insts = data[run]['simInsts']['values'][0]
                re = data[run]['system']['processor']['cores1']['core']['pseudoRetiredInsts']['values'][0]
                re_fracs[bench].append(re / (insts + re))
                LOG.info(f'\t{run} - {re_fracs[bench][-1]}')

        # 2nd pass to compute mean RE fracs
        means = []
        LOG.info('computing mean RE fractions')
        for i, run in enumerate(runs):
            fracs = []
            # collect all relative fracs for this run across all benchmarks
            for bench in re_fracs.keys():
                fracs.append(re_fracs[bench][i])
            fracs = np.array(fracs)
            means.append(np.mean(fracs))
            LOG.info(f'\t{run} - {means[-1]}')

        # plot each benchmark's relative IPCs
        xs = [0]
        bar_width = 1
        offsets = np.array([j * bar_width for j in range(len(runs))])
        assert(len(offsets) == len(runs))
        for bench in benchmarks:
            plt.bar(
                x=xs[-1] + offsets,
                height=np.array(re_fracs[bench]),
                width=bar_width,
                color=self.run_colors,
            )

            # add some bar widths of padding between each group
            new_x = xs[-1] + (bar_width * len(runs)) + 5 * bar_width
            xs.append(new_x)

        # plot geometric means
        plt.bar(
            x=xs[-1] + offsets,
            height=np.array(means),
            width=bar_width,
            color=self.run_colors,
        )

        plt.ylabel('Runahead fraction')
        plt.xticks(
            np.array(xs) + (bar_width * len(runs)) / 2,
            list(benchmarks) + ['mean'],
            rotation=90
        )
        plt.ylim(0, 0.4)

        plt.legend(
            runs,
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
