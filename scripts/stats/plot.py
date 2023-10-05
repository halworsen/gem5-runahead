from plotters.histograms.load_to_use import LoadToUse
from plotters.histograms.fetched_between_re import InterimInsts
from plotters.histograms.re_cycles import RECycles
from plotters.histograms.trigger_in_flight_cycles import TriggerInFlightCycles

from argparse import ArgumentParser
from os import makedirs
import os.path
import json

import matplotlib.pyplot as plt

ALL_PLOTS = [
    LoadToUse,
    InterimInsts,
    RECycles,
    TriggerInFlightCycles,
]

parser = ArgumentParser()
parser.add_argument(
    'statfiles', metavar='files',
    type=str,
    nargs='+',
    help='path to parsed stats json file(s)',
)
parser.add_argument(
    '--out', metavar='plots_path',
    type=str,
    default='./gem5-stats-plots',
    help='directory to output the plots in. '
         'default: ./gem5-stat-plots',
)
parser.add_argument(
    '--section',
    type=str,
    default='',
    help='if the stats have multiple sections, which one should be used. '
         'default: first section',
)

if __name__ == '__main__':
    opts = parser.parse_args()

    if not os.path.exists(opts.out):
        makedirs(opts.out)

    data = []
    for path in opts.statfiles:
        if not os.path.exists(path) or path[-5:] != '.json':
            print('Unable to find parsed stats at the given path')
            exit(1)

        with open(path, 'r') as f:
            data.append(json.load(f))
    
    # Get the stat section to use
    sections = data[0]['sectionNames']
    used_section = ''
    if opts.section:
        if not opts.section in sections:
            print('Unable to find given section in stats file')
            exit(1)
        used_section = filter(lambda s: s == opts.section, sections).__next__()
    else:
        used_section = sections[0]

    plotter_data = []
    for _d in data:
        section = filter(lambda s: s['sectionName'] == used_section, _d['sections']).__next__()
        plotter_data.append(section['stats'])

    for plotter in ALL_PLOTS:
        plotter = plotter(plotter_data)
        
        # Reset
        plt.figure()
        plt.title(plotter.name)

        # Plot
        plotter.plot()

        path = os.path.join(opts.out, f'{plotter.fname}.jpg')
        plt.savefig(path)
