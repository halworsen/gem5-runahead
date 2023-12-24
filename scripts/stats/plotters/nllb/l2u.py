from ..plotter import Plotter
from pandas import DataFrame
from pandas import concat as pdconcat
import seaborn as sbn
import matplotlib.pyplot as plt

class NLLBL2U(Plotter):
    name = 'Mean load-to-use times of normal load instructions'
    fname = 'nllb/nllb_l2u'
    description = 'Point plot of mean real load-to-use times in cycles. Error bars show mean +/- 1 standard deviation'

    def load_data(self) -> None:
        self.data = {}
        for bench in self.benchmarks():
            self.data[bench] = {}
            self.data[bench]['O3'] = self.read_stats(bench, 'm5out-spec2017-o3-baseline')
            self.data[bench]['Runahead'] = self.read_stats(bench, 'm5out-spec2017-re-baseline')
            self.data[bench]['Minimum Work'] = self.read_stats(bench, 'm5out-spec2017-re-minwork')
            self.data[bench]['NLLB'] = self.read_stats(bench, 'm5out-spec2017-re-nllb')

    def construct_frames(self) -> None:
        frame = DataFrame({
            'benchmark': [],
            'variant': [],
            'mean': [],
            'std': []
        })

        for bench in self.benchmarks():
            for variant in ['O3', 'Runahead', 'Minimum Work', 'NLLB']:
                mean, *_ = self.stat(f'{bench}.{variant}.system.processor.cores1.core.lsq0.realLoadToUse.mean')
                std, *_ = self.stat(f'{bench}.{variant}.system.processor.cores1.core.lsq0.realLoadToUse.stdev')

                row = {'benchmark': bench, 'variant': variant, 'mean': mean, 'std': std}
                frame.loc[len(frame)] = row

        frame.sort_values(by='benchmark', ascending=True, inplace=True)

        # fix for error bars
        frame = pdconcat([frame, frame])

        self.frame = frame

    def l2u_errors(self):
        # hack to allow the actual error function access to the dataframe
        def std(vec):
            idx = vec.index[0]
            frame_row = self.frame.loc[[idx]]
            frame_row = frame_row.iloc[0]

            mean = vec.iloc[0]
            stdev = frame_row['std']

            return (max(mean - stdev, 2), mean + stdev)
        return std

    def plot(self) -> None:
        _ = plt.figure(figsize=(12,5))
        sbn.set_style('whitegrid')
        plot = sbn.pointplot(
            data=self.frame,
            x='benchmark',
            y='mean',
            hue='variant',
            errorbar=self.l2u_errors(),
            capsize=.1, err_kws={'linewidth': 1},
            linestyles='',
            markers=['o', 's', 'd', '+'],
            dodge=.5,
            markersize=3,
        )

        for l in plot.get_xticklabels():
            l.set_rotation(90)
        plot.set_xlabel('')
        plot.set_ylabel('Cycles')
        plot.legend(title='CPU model')
