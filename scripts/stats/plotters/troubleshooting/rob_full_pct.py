from ..plotter import Plotter
from pandas import DataFrame
import seaborn as sbn
import matplotlib.pyplot as plt

class ROBFullPct(Plotter):
    name = 'Percentage of cycles with a full ROB'
    fname = 'debug/full_rob_pct'
    description = 'pct of cycles with a full rob'

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench] = self.read_stats(bench, 'm5out-spec2017-o3-baseline')

    def construct_frames(self) -> None:
        frame = DataFrame({
            'benchmark': [],
            'value': []
        })

        for bench in self.benchmarks():
            cycles, *_ = self.stat(f'{bench}.system.processor.cores1.core.numCycles')
            try:
                full_cycles, *_ = self.stat(f'{bench}.system.processor.cores1.core.numROBFullCycles')
            except KeyError:
                full_cycles = 0

            total_row = {'benchmark': bench, 'bucket': 'total', 'value': (full_cycles / cycles * 100)}
            frame.loc[len(frame)] = total_row

        frame.sort_values(by='benchmark', ascending=True, inplace=True)
        self.frame = frame

    def plot(self) -> None:
        sbn.set_style('whitegrid')
        ax = sbn.barplot(
            self.frame,
            x='benchmark',
            y='value',
        )

        ax.set_ylabel('Percentage')
        ax.set_xlabel('')
        for l in ax.get_xticklabels():
            l.set_rotation(90)
        # ax.set_yscale('log')
