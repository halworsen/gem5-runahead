from ..plotter import Plotter
from pandas import DataFrame
import seaborn as sbn
import scipy

class DynamicExitIPC(Plotter):
    name = 'Dynamic exit model NIPC'
    fname = 'dynamic_exit/nipc'
    description = 'Dynamic exit model NIPCs'

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            print(bench)
            self.data[bench]['Runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')
            self.data[bench]['Minimum Work'] = self.read_stats(bench, 'm5out-spec2017-re-minwork')
            self.data[bench]['NLLB'] = self.read_stats(bench, 'm5out-spec2017-re-nllb')
            self.data[bench]['Dynamic Exit'] = self.read_stats(bench, 'm5out-spec2017-re-dynamic-exit')

    def construct_frames(self) -> None:
        # just do adjusted IPC here as well
        frame = DataFrame({
            'benchmark': [],
            'variant': [],
            'insts': [],
            'cycles': [],
            'realCycles': [],
            'nipc': [],
        })

        all_insts = {'Runahead': 0, 'Minimum Work': 0, 'NLLB': 0, 'Dynamic Exit': 0}
        all_cycles = {'Runahead': 0, 'Minimum Work': 0, 'NLLB': 0, 'Dynamic Exit': 0}
        all_real_cycles = {'Runahead': 0, 'Minimum Work': 0, 'NLLB': 0, 'Dynamic Exit': 0}
        for bench in self.benchmarks():
            for model in ['Runahead', 'Minimum Work', 'NLLB', 'Dynamic Exit']:
                insts, *_ = self.stat(f'{bench}.{model}.simInsts')
                cycles, *_ = self.stat(f'{bench}.{model}.system.processor.cores1.core.numCycles')
                real_cycles, *_ = self.stat(f'{bench}.{model}.system.processor.cores1.core.realCycles')
                nipc, *_ = self.stat(f'{bench}.{model}.system.processor.cores1.core.realIpc')

                row = {
                    'benchmark': bench, 'variant': model,
                    'insts': insts, 'cycles': cycles, 'realCycles': real_cycles,
                    'nipc': nipc,
                }
                frame.loc[len(frame)] = row

                all_insts[model] += insts
                all_cycles[model] += cycles
                all_real_cycles[model] += real_cycles

        frame.sort_values(by='benchmark', ascending=True, inplace=True)

        # IPC across all benchmarks
        all_nipc = {
            'Runahead': all_insts['Runahead'] / all_real_cycles['Runahead'],
            'Minimum Work': all_insts['Minimum Work'] / all_real_cycles['Minimum Work'],
            'NLLB': all_insts['NLLB'] / all_real_cycles['NLLB'],
            'Dynamic Exit': all_insts['Dynamic Exit'] / all_real_cycles['Dynamic Exit'],
        }
        for v in all_nipc.keys():
            row = {
                'benchmark': 'all',
                'variant': v,
                'insts': all_insts[v],
                'cycles': all_cycles[v],
                'realCycles': all_real_cycles[v],
                'nipc': all_nipc[v],
            }
            frame.loc[len(frame)] = row

        print(frame)
        self.frame = frame

    def plot(self) -> None:
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='nipc',
            hue='variant'
        )

        for l in plot.get_xticklabels():
            l.set_rotation(90)
        plot.set_xlabel('')
        plot.set_ylabel('IPC')
        plot.legend(title='CPU model')
