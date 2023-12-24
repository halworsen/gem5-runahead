from ..plotter import Plotter
from pandas import DataFrame
import seaborn as sbn
import scipy

class NLLBIPC(Plotter):
    name = 'NLLB model IPC'
    fname = 'nllb/nllb_ipc'
    description = 'NLLB model IPCs'

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench]['O3'] = self.read_stats(bench, 'm5out-spec2017-o3-baseline')
            self.data[bench]['Runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')
            self.data[bench]['Minimum Work'] = self.read_stats(bench, 'm5out-spec2017-re-minwork')
            self.data[bench]['NLLB'] = self.read_stats(bench, 'm5out-spec2017-re-nllb')

    def construct_frames(self) -> None:
        # just do adjusted IPC here as well
        frame = DataFrame({
            'benchmark': [],
            'variant': [],
            'insts': [],
            'cycles': [],
            'ipc': [],
        })

        all_insts = {'O3': 0, 'Runahead': 0, 'Minimum Work': 0, 'NLLB': 0}
        all_cycles = {'O3': 0, 'Runahead': 0, 'Minimum Work': 0, 'NLLB': 0}
        for bench in self.benchmarks():
            for model in ['O3', 'Runahead', 'Minimum Work', 'NLLB']:
                insts, *_ = self.stat(f'{bench}.{model}.simInsts')
                cycles, *_ = self.stat(f'{bench}.{model}.system.processor.cores1.core.numCycles')
                ipc = insts / cycles

                row = {
                    'benchmark': bench, 'variant': model,
                    'insts': insts, 'cycles': cycles,
                    'ipc': ipc,
                }
                frame.loc[len(frame)] = row

                all_insts[model] += insts
                all_cycles[model] += cycles

        frame.sort_values(by='benchmark', ascending=True, inplace=True)

        # IPC across all benchmarks
        all_ipc = {
            'O3': all_insts['O3'] / all_cycles['O3'],
            'Runahead': all_insts['Runahead'] / all_cycles['Runahead'],
            'Minimum Work': all_insts['Minimum Work'] / all_cycles['Minimum Work'],
            'NLLB': all_insts['NLLB'] / all_cycles['NLLB'],
        }
        for v in all_ipc.keys():
            row = {
                'benchmark': 'all',
                'variant': v,
                'insts': all_insts[v],
                'cycles': all_cycles[v],
                'ipc': all_ipc[v],
            }
            frame.loc[len(frame)] = row

        print(frame[(frame['variant'] == 'NLLB') | (frame['variant'] == 'Runahead')])
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
