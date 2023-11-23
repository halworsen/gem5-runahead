from ..plotter import Plotter
from ..frame_util import FrameConstructor
import seaborn as sbn
from pandas import DataFrame 


class MinWorkRunaheadTime(Plotter):
    name = 'Instructions retired by Minimum Work Runahead'
    fname = 'min_work/minwork_runahead_time'
    description = 'Time spent in runahead as a fraction of all retired instructions'
    
    def load_data(self):
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench]['Runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')
            self.data[bench]['Minimum Work'] = self.read_stats(bench, 'm5out-spec2017-re-minwork')

    def construct_frames(self):
        frame = DataFrame({
            'benchmark': [],
            'variant': [],
            're_instructions': [],
            'normal_instructions': [],
            're_fraction': [],
            're_cycle_fraction': [],
        })

        all_re_insts = {'Runahead': 0, 'Minimum Work': 0}
        all_re_cycles = {'Runahead': 0, 'Minimum Work': 0}
        all_normal_insts = {'Runahead': 0, 'Minimum Work': 0}
        all_normal_cycles = {'Runahead': 0, 'Minimum Work': 0}
        for bench in self.benchmarks():
            for variant in self.data[bench].keys():
                normal_cycles, *_ = self.stat(f'{bench}.{variant}.system.processor.cores1.core.numCycles')
                runahead_cycles, *_ = self.stat(f'{bench}.{variant}.system.processor.cores1.core.runaheadCycles')
                normal_insts, *_ = self.stat(f'{bench}.{variant}.system.processor.cores1.core.committedInsts')
                re_insts, *_ = self.stat(f'{bench}.{variant}.system.processor.cores1.core.pseudoRetiredInsts')
                re_fraction = re_insts / (normal_insts + re_insts)
                re_cycle_fraction = runahead_cycles / (runahead_cycles + normal_cycles)
                row = {
                    'benchmark': bench,
                    'variant': variant,
                    're_instructions': re_insts, 
                    'normal_instructions': normal_insts, 
                    're_fraction': re_fraction, 
                    're_cycle_fraction': re_cycle_fraction,
                }
                frame.loc[len(frame)] = row

                all_re_insts[variant] += re_insts
                all_re_cycles[variant] += runahead_cycles
                all_normal_insts[variant] += normal_insts
                all_normal_cycles[variant] += normal_cycles

        frame.sort_values(by='benchmark', ascending=True, inplace=True)
        
        for variant in ['Runahead', 'Minimum Work']:
            all_re_fraction = all_re_insts[variant] / (all_normal_insts[variant] + all_re_insts[variant])
            all_re_cycle_fraction = all_re_cycles[variant] / (all_normal_cycles[variant] + all_re_cycles[variant])
            all_row = {
                'benchmark': 'all', 
                'variant': variant,
                're_instructions': all_re_insts[variant], 
                'normal_instructions': all_normal_insts[variant], 
                're_fraction': all_re_fraction,
                're_cycle_fraction': all_re_cycle_fraction,
            }
            frame.loc[len(frame)] = all_row

        self.frame = frame

    def plot(self):
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='re_instructions',
            hue='variant',
        )

        plot.set_ylabel('Instructions')
        plot.set_xlabel('')
        for l in plot.get_xticklabels():
            l.set_rotation(90)
        plot.set_yscale('log')

class MinWorkRunaheadFraction(MinWorkRunaheadTime):
    name = 'Fraction of instructions that are runahead' 
    fname = 'min_work/minwork_runahead_fraction'
    description = 'Fraction of instructions that are runahead'

    def plot(self):
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='re_fraction',
            hue='variant',
        )

        plot.set_ylabel('Fraction')
        plot.set_xlabel('')
        for l in plot.get_xticklabels():
            l.set_rotation(90)

class MinWorkRunaheadCycleFraction(MinWorkRunaheadTime):
    name = 'Fraction of cycles that are runahead (Min Work model)' 
    fname = 'min_work/minwork_runahead_periods'
    description = 'Fraction of cycles that are runahead'

    def plot(self):
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='re_cycle_fraction',
            hue='variant',
        )

        plot.set_ylabel('Fraction')
        plot.set_xlabel('')
        for l in plot.get_xticklabels():
            l.set_rotation(90)

class MinWorkRunaheadPeriods(Plotter):
    name = 'Total amount of runahead periods'
    fname = 'min_work/minwork_runahead_periods'
    description = 'Number of runahead periods encountered by the runahead baseline and min work model'

    def load_data(self):
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench]['Runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')
            self.data[bench]['Minimum Work'] = self.read_stats(bench, 'm5out-spec2017-re-minwork')

    def construct_frames(self):
        self.frame = FrameConstructor.value_frame(
            self.data,
            'system.processor.cores1.core.runaheadPeriods',
        )
        self.frame.sort_values(by='benchmark', ascending=True, inplace=True)

    def plot(self):
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='value',
            hue='experiment',
        )

        plot.set_ylabel('Runahead periods')
        plot.set_xlabel('')
        for l in plot.get_xticklabels():
            l.set_rotation(90)
        plot.set_yscale('log')

        plot.legend(title='CPU Model')
