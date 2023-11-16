from ..plotter import Plotter
from pandas import DataFrame
import seaborn as sbn

class BaselineIPC(Plotter):
    name = 'Runahead baseline IPC'
    fname = 'baseline_ipc'
    description = 'Runahead baseline model IPC compared against the stock O3CPU'

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench]['stock'] = self.read_stats(bench, 'm5out-spec2017-o3-baseline')
            self.data[bench]['runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')

    def construct_frames(self) -> None:
        # just do adjusted IPC here as well
        frame = DataFrame({
            'benchmark': [],
            'variant': [],
            'insts': [],
            'cycles': [],
            'adjusted_cycles': [],
            'ipc': [],
            'adjusted_ipc': [],
        })

        all_insts = {'O3': 0, 'Runahead': 0, 'Adjusted RE': 0}
        all_cycles = {'O3': 0, 'Runahead': 0, 'Adjusted RE': 0}
        for bench in self.benchmarks():
            stock_insts, *_ = self.stat(f'{bench}.stock.simInsts')
            stock_cycles, *_ = self.stat(f'{bench}.stock.system.processor.cores1.core.numCycles')
            stock_ipc = stock_insts / stock_cycles

            row = {
                'benchmark': bench,
                'variant': 'O3',
                'insts': stock_insts,
                'cycles': stock_cycles,
                'ipc': stock_ipc,
                'adjusted_ipc': stock_ipc,
            }
            frame.loc[len(frame)] = row

            all_insts['O3'] += stock_insts
            all_cycles['O3'] += stock_cycles

            re_insts, *_ = self.stat(f'{bench}.runahead.simInsts')
            re_cycles, *_ = self.stat(f'{bench}.runahead.system.processor.cores1.core.numCycles')
            overhead_cycles, *_ = self.stat(f'{bench}.runahead.system.processor.cores1.core.commit.totalRunaheadOverhead')
            adjusted_cycles = re_cycles - overhead_cycles

            ipc = re_insts / re_cycles
            adjusted_ipc = re_insts / adjusted_cycles

            row = {
                'benchmark': bench,
                'variant': 'Runahead',
                'insts': re_insts,
                'cycles': re_cycles,
                'adjusted_cycles': adjusted_cycles,
                'ipc': ipc,
                'adjusted_ipc': adjusted_ipc
            }
            frame.loc[len(frame)] = row

            all_insts['Runahead'] += re_insts
            all_cycles['Runahead'] += re_cycles
            all_insts['Adjusted RE'] += re_insts
            all_cycles['Adjusted RE'] += adjusted_cycles

        frame.sort_values(by='benchmark', ascending=True, inplace=True)

        # IPC across all benchmarks
        all_ipc = {
            'O3': all_insts['O3'] / all_cycles['O3'],
            'Runahead': all_insts['Runahead'] / all_cycles['Runahead'],
        }
        all_adjusted_ipc = {
            'O3': all_ipc['O3'],
            'Runahead': all_insts['Adjusted RE'] / all_cycles['Adjusted RE'],
        }
        for v in all_ipc.keys():
            row = {
                'benchmark': 'all',
                'variant': v,
                'insts': all_insts[v],
                'cycles': all_cycles[v],
                'adjusted_cycles': all_cycles[v],
                'ipc': all_ipc[v],
                'adjusted_ipc': all_adjusted_ipc[v],
            }
            frame.loc[len(frame)] = row

        self.frame = frame

    def plot(self) -> None:
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='ipc',
            hue='variant'
        )

        for l in plot.get_xticklabels():
            l.set_rotation(90)
        plot.set_xlabel('')
        plot.set_ylabel('IPC')
        plot.legend(title='CPU model')

class BaselineAdjustedIPC(BaselineIPC):
    name = 'Runahead baseline overhead-adjusted IPC'
    fname = 'baseline_adjusted_ipc'
    description = 'Overhead-adjusted IPC of the runahead baseline model compared to the stock O3CPU'

    def plot(self) -> None:
        sbn.set_style('whitegrid')

        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='adjusted_ipc',
            hue='variant'
        )

        for l in plot.get_xticklabels():
            l.set_rotation(90)
        plot.set_xlabel('')
        plot.set_ylabel('IPC')
        plot.legend(title='CPU model')
