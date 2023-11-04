'''
Histogram displaying cycles spent in runahead
'''

from .plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np
import os

class SimPointWeights(Plotter):
    name = 'SPEC2017 benchmark SimPoint weights'
    fname = 'sp_weights'
    description = 'SimPoint weights for each benchmark'

    def load_data(self) -> None:
        self.data = {}
        for bench_dir in os.scandir(self.log_dir):
            if bench_dir.name not in self.valid_benchmarks:
                continue
            self.data[bench_dir.name] = []

            weights_path = os.path.join(bench_dir.path, 'm5out-gem5-spec2017-simpoint', 'simpoint', 'weights.txt')
            with open(weights_path) as f:
                weightings = [l.split() for l in f.readlines()]
                for sp_weight in weightings:
                    self.data[bench_dir.name].append(float(sp_weight[0]))

    def plot(self) -> None:
        benchmarks = tuple(self.data.keys())

        xs = [0]
        bar_width = 1
        for _, weights in self.data.items():
            offsets = np.array([j * bar_width for j in range(len(weights))])
            colors = ['black' if w == max(weights) else 'gray' for w in weights]

            assert(len(offsets) == len(weights))
            plt.bar(
                x=xs[-1] + offsets,
                height=np.array(weights) * 100,
                width=bar_width,
                color=colors,
            )

            # add some bar widths of padding between each group
            new_x = xs[-1] + (bar_width * len(weights)) + 5 * bar_width
            xs.append(new_x)
        
        tick_offsets = [(len(weights) * bar_width) / 2 for weights in self.data.values()]

        plt.ylabel('SimPoint Weight (%)')
        plt.xticks(np.array(xs[:-1]) + np.array(tick_offsets), benchmarks, rotation=90)
        plt.ylim(0, 100)
