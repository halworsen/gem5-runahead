from ..plotter import Plotter
from pandas import DataFrame
import seaborn as sbn
import matplotlib.pyplot as plt

class BaselineRunaheadOverhead(Plotter):
    name = 'Overhead of entering and exiting runahead'
    fname = 'baseline/baseline_runahead_overhead'
    description = 'The cycle overhead of entering and exiting runahead for every benchmark'

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench] = self.read_stats(bench, 'm5out-spec2017-re-baseline')

    def construct_enter_frame(self) -> None:
        frame = DataFrame({
            'benchmark': [],
            'bucket': [],
            'value': []
        })

        for bench in self.benchmarks():
            distribution = self.stat(
                f'{bench}.system.processor.cores1.core.commit.runaheadEnterOverhead',
                read_values=False,
            )
            for bucket in distribution.keys():
                bkt_val, *_ = self.stat(f'{bench}.system.processor.cores1.core.commit.runaheadEnterOverhead.{bucket}')

                row = {'benchmark': bench, 'bucket': bucket, 'value': bkt_val}
                frame.loc[len(frame)] = row

            total_overhead, *_ = self.stat(f'{bench}.system.processor.cores1.core.commit.totalRunaheadEnterOverhead')
            total_row = {'benchmark': bench, 'bucket': 'total', 'value': total_overhead}
            frame.loc[len(frame)] = total_row

        frame.sort_values(by='benchmark', ascending=True, inplace=True)
        self.enter_frame = frame

    def construct_exit_frame(self) -> None:
        frame = DataFrame({
            'benchmark': [],
            'bucket': [],
            'value': []
        })

        for bench in self.benchmarks():
            distribution = self.stat(
                f'{bench}.system.processor.cores1.core.commit.runaheadExitOverhead',
                read_values=False,
            )
            for bucket in distribution.keys():
                bkt_val, *_ = self.stat(f'{bench}.system.processor.cores1.core.commit.runaheadExitOverhead.{bucket}')

                row = {'benchmark': bench, 'bucket': bucket, 'value': bkt_val}
                frame.loc[len(frame)] = row

            total_overhead, *_ = self.stat(f'{bench}.system.processor.cores1.core.commit.totalRunaheadExitOverhead')
            total_row = {'benchmark': bench, 'bucket': 'total', 'value': total_overhead}
            frame.loc[len(frame)] = total_row

        frame.sort_values(by='benchmark', ascending=True, inplace=True)
        self.exit_frame = frame

    def construct_frames(self) -> None:
        self.construct_enter_frame()
        self.construct_exit_frame()

        # Read total overhead as well (enter + exit)
        total_frame = DataFrame({
            'benchmark': [],
            'cycles': [],
        })

        for bench in self.benchmarks():
            total_overhead, *_ = self.stat(
                'system.processor.cores1.core.commit.totalRunaheadOverhead',
                data=self.data[bench],
            )
            row = {'benchmark': bench, 'cycles': total_overhead}
            total_frame.loc[len(total_frame)] = row
        
        total_frame.sort_values(by='benchmark', ascending=True, inplace=True)
        self.total_frame = total_frame

    def plot(self) -> None:
        _, ax = plt.subplots()

        sbn.set_style('whitegrid')

        # Stack them up by overlaying
        total = sbn.barplot(
            self.total_frame,
            x='benchmark',
            y='cycles',
            errorbar=None,
            ax=ax,
        )

        _ = sbn.barplot(
            self.exit_frame[self.exit_frame['bucket'] == 'total'],
            x='benchmark',
            y='value',
            errorbar=None,
            ax=ax,
        )

        _ = sbn.barplot(
            self.enter_frame[self.enter_frame['bucket'] == 'total'],
            x='benchmark',
            y='value',
            errorbar=None,
            ax=ax,
        )

        ax.set_ylabel('Cycles')
        ax.set_xlabel('')
        for l in ax.get_xticklabels():
            l.set_rotation(90)
        ax.set_yscale('log')

        ax.legend(['Total', 'Exit', 'Enter'])
