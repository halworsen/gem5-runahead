'''
Base class for plot producers
'''

import os
import json
from typing import Any

class Plotter:
    name = 'base plotter class'
    ''' Pretty name for the figure '''
    fname = 'baseplotterclass.jpg'
    ''' Filename of the output plot '''
    description = 'base plotter class'
    ''' Description of what kind of plot this plotter makes '''
    manual = False
    ''' Whether this plotter will do everything manually, including saving the plot '''

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

    def read_stats(self, bench: str, run_name: str) -> dict:
        stats_path = os.path.join(self.log_dir, bench, run_name, self.stat_file)
        with open(stats_path, 'r') as f:
            data = json.load(f)
        return data['sections'][0]['stats']

    def benchmarks(self) -> tuple:
        '''
        Get a list of all valid benchmarks in the log directory
        '''
        return (d.name for d in os.scandir(self.log_dir) if d.name in self.valid_benchmarks)

    def experiments(self, benchmark: str) -> tuple:
        '''
        Get a list of all experiments belonging to a benchmark
        '''
        path = os.path.join(self.log_dir, benchmark)
        return (d.name for d in os.scandir(path))

    def stat(
            self,
            path: str,
            read_values: bool = True,
            data: dict = None,
    ) -> Any:
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
        if read_values:
            val = tuple(val['values'])
        return val

    def load_data(self) -> None:
        '''
        Load any data required to produce the plot
        '''
        pass

    def construct_frames(self) -> None:
        '''
        Construct the dataframe to use for the plot
        '''
        pass

    def plot(self) -> None:
        '''
        Generate a plot using the stored data
        '''
        pass
