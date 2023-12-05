from ..plotter import Plotter
from pandas import DataFrame, Series
import seaborn as sbn
import numpy as np
import scipy


class DynExInterimPeriods(Plotter):
    name = 'Instructions retired by interim periods between runahead (Dynamic exit model)'
    fname = 'dynamic_exit/dynex_interim_periods'
    description = 'Histograms of interim period lengths for every benchmark'

    # Any bucket corresponding to >= this cycle count counts as an overflow
    overflow_limit = 150

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = self.read_stats(bench, 'm5out-spec2017-re-dynamic-exit')

    def extract_buckets(self, dist: dict) -> list[str]:
        dist_buckets = []
        for key in dist.keys():
            if '-' in key or key.isdigit():
                dist_buckets.append(key)
        return dist_buckets

    @staticmethod
    def frame_sort(series) -> Series:
        keys = []
        for x in series:
            if '-' not in x and '>=' not in x:
                return series

            if '>=' in x:
                keys.append(np.inf)
            else:
                keys.append(int(x.split('-')[1]))
        return Series(keys)

    def construct_frames(self) -> None:
        frame = DataFrame({
            'benchmark': [],
            'bucket': [],
            'percent': [],
            'count': []
        })
        overflow_bkt = f'>={self.overflow_limit}'
        for bench in self.benchmarks():
            overflow_pct = 0
            overflow_cnt = 0

            distribution = self.stat(
                'system.processor.cores1.core.instsRetiredBetweenRunahead',
                read_values=False,
                data=self.data[bench],
            )
            dist_buckets = self.extract_buckets(distribution)

            for bkt in dist_buckets:
                val_cnt, val_pct, *_ = self.stat(
                    f'system.processor.cores1.core.instsRetiredBetweenRunahead.{bkt}',
                    data=self.data[bench],
                )

                if '-' in bkt:
                    _, end = map(int, bkt.split('-'))
                else:
                    end = int(bkt)

                if end >= self.overflow_limit:
                    overflow_pct += val_pct
                    overflow_cnt += val_cnt
                else:
                    row = {'benchmark': bench, 'bucket': bkt, 'percent': val_pct * 100, 'count': val_cnt}
                    frame.loc[len(frame)] = row

            overflows_cnt, overflows_pct, *_ = self.stat(
                'system.processor.cores1.core.instsRetiredBetweenRunahead.overflows',
                data=self.data[bench],
            )
            overflow_cnt += overflows_cnt
            overflow_pct += overflows_pct

            row = {'benchmark': bench, 'bucket': overflow_bkt, 'percent': overflow_pct * 100, 'count': overflow_cnt}
            frame.loc[len(frame)] = row

        frame.sort_values(
            by=['benchmark', 'bucket'],
            ascending=[True, False],
            key=self.frame_sort,
            inplace=True
        )

        # all combined
        total_periods = np.sum(frame['count'])
        for bkt in frame['bucket'].unique():
            rows = frame[frame['bucket'] == bkt]
            rows = rows[rows['benchmark'] != 'all']

            bkt_periods = np.sum(rows['count'])
            all_pct = (bkt_periods / total_periods) * 100

            mean_row = {'benchmark': 'all', 'bucket': bkt, 'percent': all_pct, 'count': bkt_periods}
            frame.loc[len(frame)] = mean_row

        self.frame = frame

    def plot(self) -> None:
        sbn.set_style('whitegrid')
        ax = sbn.histplot(
            self.frame,
            x='benchmark',
            hue='bucket',
            weights='percent',
            multiple='stack',
            linewidth=.5,
        )

        for l in ax.get_xticklabels():
            l.set_rotation(90)
        ax.set_xlabel('')
        ax.set_xlim(-0.51, 16.5)
        ax.set_ylim(0, 100.0)
        ax.set_ylabel('Percent of interim periods')

        yticks = ax.get_yticks()
        yticks = list(map(lambda t: f'{t:.0f}%', yticks))
        # supress a warning
        ax.set_yticks(ax.get_yticks())
        # then add the % to the y ticks
        ax.set_yticklabels(yticks)

        leg = ax.get_legend()
        leg.set_title('Instructions retired')
