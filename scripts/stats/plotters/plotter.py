'''
Base class for plot producers
'''

import os
import json
import re
from typing import Any
import numpy as np
import distinctipy

class Plotter:
    name = 'base plotter class'
    ''' Pretty name for the figure '''
    fname = 'baseplotterclass.jpg'
    ''' Filename of the output plot '''
    description = 'base plotter class'
    ''' Description of what kind of plot this plotter makes '''

    log_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'
    stat_file = 'gem5stats.json'

    valid_benchmarks = (
        "perlbench_s_0",
        "perlbench_s_1",
        "perlbench_s_2",
        "gcc_s_1",
        "gcc_s_2",
        "mcf_s_0",
        "cactuBSSN_s_0",
        "omnetpp_s_0",
        "wrf_s_0",
        "xalancbmk_s_0",
        "x264_s_0",
        "x264_s_2",
        "imagick_s_0",
        "nab_s_0",
        "exchange2_s_0",
        "fotonik3d_s_0",
    )

    def __init__(self):
        # Pre-compute some highly distinct colors
        self.color_pool = distinctipy.get_colors(10, n_attempts=20000, rng=1)

    def read_stats(self, bench: str, run_name: str) -> dict:
        stats_path = os.path.join(self.log_dir, bench, run_name, self.stat_file)
        with open(stats_path, 'r') as f:
            data = json.load(f)
        return data['sections'][0]['stats']

    def benchmarks(self) -> tuple:
        '''
        Get a list of all benchmarks in the log directory
        '''
        return (d.name for d in os.scandir(self.log_dir) if d.name in self.valid_benchmarks)

    def experiments(self, benchmark: str) -> tuple:
        '''
        Get a list of all experiments belonging to a benchmark
        '''
        path = os.path.join(self.log_dir, benchmark)
        return (d.name for d in os.scandir(path))

    def read_stat(self, path: str, val_num: int = 0, data: dict = None) -> Any:
        '''
        Read a single value at the given stat path in the given data.
        '''
        if data == None:
            data = self.data

        keys = path.split('.')
        val = data
        while len(keys):
            key = keys[0]
            val = val[key]
            keys = keys[1:]
        val = val['values'][val_num]
        return val

    def plot_grouped_bars(self, data: dict, plot: Any) -> None:
        '''
        Plot grouped bars for each key in a data dict.
        The data must be a dict with benchmark names as keys. The values should be the bar heights.
        '''
        xs = [0]
        keys = list(data.keys())
        amount = len(data[keys[0]])
        colors = self.color_pool[:amount]
        for heights in data.values():
            offsets = np.array(range(amount))
            plot.bar(
                x=xs[-1] + offsets,
                height=np.array(heights),
                width=1,
                color=colors,
            )

            # add some bar widths of padding between each group
            new_x = xs[-1] + amount + 5
            xs.append(new_x)
        xs = xs[:-1]

        tick_centering = [len(data[key]) / 2 for key in data.keys()]
        set_ticks_func = plot.xticks if hasattr(plot, 'xticks') else plot.set_xticks
        set_ticks_func(
            ticks=np.array(xs) + np.array(tick_centering),
            labels=list(data.keys()),
            rotation=90
        )

    def load_data(self) -> None:
        '''
        Load any data required to produce the plot
        '''
        pass

    def plot(self) -> None:
        '''
        Generate a plot using the stored data
        '''
        pass
