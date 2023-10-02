from plotters.histograms.load_to_use import LoadToUse
from plotters.histograms.fetched_between_re import InterimInsts
from plotters.histograms.re_cycles import RECycles

from argparse import ArgumentParser
from os import makedirs
import os.path
import json

import matplotlib.pyplot as plt

ALL_PLOTS = [
    # LoadToUse,
    InterimInsts,
    RECycles,
]

parser = ArgumentParser()
parser.add_argument(
    'statfile', metavar='file',
    type=str,
    help='path to parsed stats json file',
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

    if not os.path.exists(opts.statfile) or opts.statfile[-5:] != '.json':
        print('Unable to find parsed stats at the given path')
        exit(1)

    if not os.path.exists(opts.out):
        makedirs(opts.out)

    data = None
    with open(opts.statfile, 'r') as f:
        data = json.load(f)
    
    # Get the stat section to use
    sections = data['sectionNames']
    used_section = ''
    if opts.section:
        if not opts.section in sections:
            print('Unable to find given section in stats file')
            exit(1)
        used_section = filter(lambda s: s == opts.section, sections).__next__()
    else:
        used_section = sections[0]

    plotter_data = filter(lambda s: s['sectionName'] == used_section, data['sections']).__next__()
    plotter_data = plotter_data['stats']
    for plotter in ALL_PLOTS:
        plotter = plotter(plotter_data)
        
        # Reset
        plt.figure()
        plt.title(plotter.name)

        # Plot
        plotter.plot()

        path = os.path.join(opts.out, f'{plotter.fname}.jpg')
        plt.savefig(path)
