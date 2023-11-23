from ..plotter import Plotter
from pandas import DataFrame
from pandas import concat as pdconcat
import seaborn as sbn
import logging
import re
import matplotlib.pyplot as plt

LOG = logging.getLogger(__name__)

class MinimumWorkSensitivityIPC(Plotter):
    name = 'Minimum work model IPC (Sorted by IPC)'
    fname = 'min_work/minimum_work_ipc'
    description = 'Minimum work model IPC compared against the runahead baseline'

    def __init__(self, deadline_pat: str, work_pat: str) -> None:
        self.deadline_pat = re.compile(deadline_pat)
        self.work_pat = re.compile(work_pat)

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench]['stock'] = self.read_stats(bench, 'm5out-spec2017-o3-baseline')
            self.data[bench]['runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')
            self.data[bench]['minwork'] = {'deadline': {}, 'work': {}}
                
            # Find all the In-Flight Threshold simulation outputs
            LOG.debug('reading experiment statistics')
            for run in self.experiments(bench):
                match = self.work_pat.match(run)
                if not match:
                    match = self.deadline_pat.match(run)
                if not match:
                    continue
                variable = match.group(1)
                value = match.group(2)
                
                # Read their stats
                LOG.debug(f'\t reading stats: {run}')
                self.data[bench]['minwork'][variable][value] = self.read_stats(bench, run)

    def construct_frames(self) -> None:
        frame = DataFrame({
            'benchmark': [],
            'experiment': [],
            'experiment_variable': [],
            'variant': [],
            'insts': [],
            'cycles': [],
            'ipc': [],
        })

        all_insts = {'O3': 0, 'Minimum Work': 0, 'Runahead': 0}
        all_cycles = {'O3': 0, 'Minimum Work': 0, 'Runahead': 0}
        for bench in self.benchmarks():
            stock_insts, *_ = self.stat(f'{bench}.stock.simInsts')
            stock_cycles, *_ = self.stat(f'{bench}.stock.system.processor.cores1.core.numCycles')
            stock_ipc = stock_insts / stock_cycles

            row = {
                'benchmark': bench,
                'experiment': 'O3',
                'experiment_variable': 'O3',
                'variant': 'O3',
                'insts': stock_insts,
                'cycles': stock_cycles,
                'ipc': stock_ipc,
            }
            frame.loc[len(frame)] = row

            all_insts['O3'] += stock_insts
            all_cycles['O3'] += stock_cycles
            
            re_insts, *_ = self.stat(f'{bench}.runahead.simInsts')
            re_cycles, *_ = self.stat(f'{bench}.runahead.system.processor.cores1.core.numCycles')

            ipc = re_insts / re_cycles

            row = {
                'benchmark': bench,
                'experiment': 'runahead',
                'experiment_variable': 'runahead',
                'variant': 'Runahead',
                'insts': re_insts,
                'cycles': re_cycles,
                'ipc': ipc,
            }
            frame.loc[len(frame)] = row

            all_insts['Runahead'] += re_insts
            all_cycles['Runahead'] += re_cycles

            for variable, variable_data in self.data[bench]['minwork'].items():
                for run in variable_data.keys():
                    min_work_insts, *_ = self.stat(f'{bench}.minwork.{variable}.{run}.simInsts')
                    min_work_cycles, *_ = self.stat(f'{bench}.minwork.{variable}.{run}.system.processor.cores1.core.numCycles')
                    min_work_ipc = min_work_insts / min_work_cycles

                    row = {
                        'benchmark': bench,
                        'experiment': int(run),
                        'experiment_variable': variable,
                        'variant': f'Min Work - {variable}={run}',
                        'insts': min_work_insts,
                        'cycles': min_work_cycles,
                        'ipc': min_work_ipc,
                    }
                    frame.loc[len(frame)] = row

        # IPC across all benchmarks
        all_ipc = {
            'O3': all_insts['O3'] / all_cycles['O3'],
            'Runahead': all_insts['Runahead'] / all_cycles['Runahead'],
        }
        for v in all_ipc.keys():
            row = {
                'benchmark': 'all',
                'experiment': v,
                'experiment_variable': 'all',
                'variant': v,
                'insts': all_insts[v],
                'cycles': all_cycles[v],
                'ipc': all_ipc[v],
            }
            frame.loc[len(frame)] = row

        # Compute aggregate stats for minimum work
        # Roundabout way of selecting minimum work rows only
        selector = (frame['experiment'] != 'runahead') & (frame['experiment'] != 'O3')
        min_work_frame = frame[selector]
        # Start with deadline and work runs
        for variable in min_work_frame['experiment_variable'].unique():
            selector = (min_work_frame['benchmark'] != 'all') & (min_work_frame['experiment_variable'] == variable)
            variable_frame = min_work_frame[selector]
            # Then the different values for each of the parameters
            for run in variable_frame['experiment'].unique():
                run_frame = variable_frame[variable_frame['experiment'] == run]
                total_insts = run_frame['insts'].sum()
                total_cycles = run_frame['cycles'].sum()
                total_ipc = total_insts / total_cycles

                row = {
                    'benchmark': 'all',
                    'experiment': run,
                    'experiment_variable': variable,
                    'variant': f'Min Work - {variable}={run}',
                    'insts': total_insts,
                    'cycles': total_cycles,
                    'ipc': total_ipc,
                }

                frame.loc[len(frame)] = row

        # print(frame[frame['benchmark'] == 'all'].sort_values(by='ipc', ascending=False))
        frame.sort_values(
            by=['ipc', 'benchmark'], 
            ascending=[True, True],
            inplace=True
        )
        all_rows = frame[frame['benchmark'] == 'all']
        all_but_all_rows = frame[frame['benchmark'] != 'all']
        frame = pdconcat([all_but_all_rows, all_rows])

        self.frame = frame

    def plot(self) -> None:
        fig = plt.figure(figsize=(16, 5))
        sbn.set_style('whitegrid')
        sbn.set_palette('Paired', n_colors=12)

        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='ipc',
            hue='variant',
            errorbar=None,
        )

        for l in plot.get_xticklabels():
                l.set_rotation(90)

        plot.legend(title='CPU model', bbox_to_anchor=(1, 1))
        plot.set_ylabel('IPC')
        plot.set_xlabel('')


class MinimumWorkIPC(Plotter):
    name = 'Minimum work model IPC (Sorted by IPC)'
    fname = 'min_work/minimum_work_final_model_ipc'
    description = 'Minimum work model IPC compared against the runahead baseline'

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench]['O3'] = self.read_stats(bench, 'm5out-spec2017-o3-baseline')
            self.data[bench]['Runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')
            self.data[bench]['Minimum Work'] = self.read_stats(bench, 'm5out-spec2017-re-minwork')

    def construct_frames(self):
        frame = DataFrame({
            'benchmark': [],
            'variant': [],
            'insts': [],
            'cycles': [],
            'ipc': [],
        })

        all_insts = {'O3': 0, 'Runahead': 0, 'Minimum Work' : 0}
        all_cycles = {'O3': 0, 'Runahead': 0, 'Minimum Work' : 0}
        for bench in self.benchmarks():
            variants = list(all_insts.keys())
            for variant in variants:
                variant_insts, *_ = self.stat(f'{bench}.{variant}.simInsts')
                variant_cycles, *_ = self.stat(f'{bench}.{variant}.system.processor.cores1.core.numCycles')
                variant_ipc = variant_insts / variant_cycles

                row = {
                    'benchmark': bench,
                    'variant': variant,
                    'insts': variant_insts,
                    'cycles': variant_cycles,
                    'ipc': variant_ipc,
                }
                frame.loc[len(frame)] = row

                all_insts[variant] += variant_insts
                all_cycles[variant] += variant_cycles

        frame.sort_values(by='benchmark', ascending=True)

        # IPC across all benchmarks
        all_ipc = {
            'O3': all_insts['O3'] / all_cycles['O3'],
            'Runahead': all_insts['Runahead'] / all_cycles['Runahead'],
            'Minimum Work': all_insts['Minimum Work'] / all_cycles['Minimum Work'],
        }
        for variant in all_ipc.keys():
            row = {
                'benchmark': 'all',
                'variant': variant,
                'insts': all_insts[variant],
                'cycles': all_cycles[variant],
                'ipc': all_ipc[variant],
            }
            frame.loc[len(frame)] = row

        self.frame = frame
            
    def plot(self) -> None:
        fig = plt.figure(figsize=(16, 5))
        sbn.set_style('whitegrid')

        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='ipc',
            hue='variant',
            errorbar=None,
        )

        for l in plot.get_xticklabels():
                l.set_rotation(90)

        plot.legend(title='CPU model', bbox_to_anchor=(1, 1))
        plot.set_ylabel('IPC')
        plot.set_xlabel('')
