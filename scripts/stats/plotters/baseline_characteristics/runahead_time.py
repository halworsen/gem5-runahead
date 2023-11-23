from ..plotter import Plotter
import seaborn as sbn
from pandas import DataFrame 


class BaselineRunaheadTime(Plotter):
    name = 'Instructions retired by Runahead'
    fname = 'baseline/baseline_runahead_time'
    description = 'Time spent in runahead as a fraction of all retired instructions'
    
    def load_data(self):
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench] = self.read_stats(bench, 'm5out-spec2017-re-baseline')

    def construct_frames(self):
        frame = DataFrame({
            'benchmark': [],
            're_instructions': [],
            'normal_instructions': [],
            're_fraction': [],
            're_cycle_fraction': [],
        })

        all_re_insts = 0
        all_re_cycles = 0
        all_normal_insts = 0
        all_normal_cycles = 0
        for bench in self.benchmarks():
            normal_cycles, *_ = self.stat(f'{bench}.system.processor.cores1.core.numCycles')
            runahead_cycles, *_ = self.stat(f'{bench}.system.processor.cores1.core.runaheadCycles')
            normal_insts, *_ = self.stat(f'{bench}.system.processor.cores1.core.committedInsts')
            re_insts, *_ = self.stat(f'{bench}.system.processor.cores1.core.pseudoRetiredInsts')
            re_fraction = re_insts / (normal_insts + re_insts)
            re_cycle_fraction = runahead_cycles / (runahead_cycles + normal_cycles)
            row = {
                'benchmark': bench, 
                're_instructions': re_insts, 
                'normal_instructions': normal_insts, 
                're_fraction': re_fraction, 
                're_cycle_fraction': re_cycle_fraction,
            }
            frame.loc[len(frame)] = row

            all_re_insts += re_insts
            all_re_cycles += runahead_cycles
            all_normal_insts += normal_insts
            all_normal_cycles += normal_cycles

        frame.sort_values(by='benchmark', ascending=True, inplace=True)
        
        all_re_fraction = all_re_insts / (all_normal_insts + all_re_insts)
        all_re_cycle_fraction = all_re_cycles / (all_normal_cycles + all_re_cycles)
        all_row = {
            'benchmark': 'all', 
            're_instructions': all_re_insts, 
            'normal_instructions': all_normal_insts, 
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
            y='re_instructions'
        )
        
        plot.set_ylabel('Instructions')
        plot.set_xlabel('')
        for l in plot.get_xticklabels():
            l.set_rotation(90)
        plot.set_yscale('log')

class BaselineRunaheadFraction(BaselineRunaheadTime):
    name = 'Fraction of instructions that are runahead' 
    fname = 'baseline/baseline_runahead_fraction'
    description = 'Fraction of instructions that are runahead'

    def plot(self):
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='re_fraction',
        )
        
        plot.set_ylabel('Fraction')
        plot.set_xlabel('')
        for l in plot.get_xticklabels():
            l.set_rotation(90)

class BaselineRunaheadCycleFraction(BaselineRunaheadTime):
    name = 'Fraction of cycles that are runahead' 
    fname = 'baseline/baseline_runahead_fraction_cycles'
    description = 'Fraction of cycles that are runahead'

    def plot(self):
        sbn.set_style('whitegrid')
        plot = sbn.barplot(
            self.frame,
            x='benchmark',
            y='re_cycle_fraction',
        )
        
        plot.set_ylabel('Fraction')
        plot.set_xlabel('')
        for l in plot.get_xticklabels():
            l.set_rotation(90)
    