from typing import Any
from .plotter import Plotter
from pandas import DataFrame
import scipy
import numpy as np

def read_stat(data: dict, path: str, val_num: int = 0) -> Any:
    '''
    Read a single value at the given stat path in the given data.
    '''
    keys = path.split('.')
    val = data
    while len(keys):
        key = keys[0]
        val = val[key]
        keys = keys[1:]
    val = val['values'][val_num]
    return val

class FrameConstructor:
    stat = Plotter.stat

    @staticmethod
    def relative_frame(
            data: dict,
            stat: str,
            relative_to: str = 'baseline'
    ) -> DataFrame:
        '''
        Construct a dataframe with relative values to a specific experiment
        for every benchmark in the dataset
        '''
        frame = DataFrame({
            'benchmark': [],
            'experiment': [],
            'value': [],
        })

        # Compute the relative stats
        for bench, bench_data in data.items():
            base_stat = read_stat(bench_data[relative_to], stat)
            all_experiments = sorted([exp for exp in bench_data.keys() if exp != relative_to])
            for experiment in all_experiments:
                experiment_stat = read_stat(bench_data[experiment], stat)
                relative_stat = experiment_stat / base_stat

                row = {'benchmark': bench, 'experiment': experiment, 'value': relative_stat}
                frame.loc[len(frame)] = row

        return frame

    @staticmethod
    def value_frame(
        data: dict,
        stat: str,
        exclude: list = None,
    ) -> DataFrame:
        '''
        Construct a dataframe with the values of a given statistic for all experiments, for all benchmarks
        '''
        frame = DataFrame({
            'benchmark': [],
            'experiment': [],
            'value': [],
        })

        # Compute the relative stats
        for bench, bench_data in data.items():
            all_experiments = sorted(list(bench_data.keys()))
            if exclude:
                all_experiments = sorted([exp for exp in bench_data.keys() if exp not in exclude])
            for experiment in all_experiments:
                val = read_stat(bench_data[experiment], stat)
                row = {'benchmark': bench, 'experiment': experiment, 'value': val}
                frame.loc[len(frame)] = row

        return frame

class FrameMeans:
    @staticmethod
    def add_mean(frame: DataFrame) -> None:
        '''
        Modifies a dataframe with rows containing the mean value of each unique experiment
        for every benchmark present in the dataframe
        '''
        for experiment in frame['experiment'].unique():
            val_row = frame[frame['benchmark'] != 'mean']
            val_row = val_row[val_row['experiment'] == experiment]['value']

            mean = np.mean(val_row)
            row = {'benchmark': 'mean', 'experiment': experiment, 'value': mean}
            frame.loc[len(frame)] = row

    @staticmethod
    def add_gmean(frame: DataFrame) -> None:
        '''
        Modifies a dataframe with rows containing the geometric mean value of each unique experiment
        for every benchmark present in the dataframe
        '''
        for experiment in frame['experiment'].unique():
            val_row = frame[frame['benchmark'] != 'mean']
            val_row = val_row[val_row['experiment'] == experiment]['value']

            mean = scipy.stats.gmean(val_row)
            row = {'benchmark': 'gmean', 'experiment': experiment, 'value': mean}
            frame.loc[len(frame)] = row

    @staticmethod
    def add_hmean(frame: DataFrame) -> None:
        '''
        Modifies a dataframe with rows containing the harmonic mean value of each unique experiment
        for every benchmark present in the dataframe
        '''
        for experiment in frame['experiment'].unique():
            val_row = frame[frame['benchmark'] != 'mean']
            val_row = val_row[val_row['experiment'] == experiment]['value']

            mean = scipy.stats.hmean(val_row)
            row = {'benchmark': 'hmean', 'experiment': experiment, 'value': mean}
            frame.loc[len(frame)] = row
