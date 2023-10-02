'''
Histogram displaying cycles spent in runahead
'''

from ..plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np

class RECycles(Plotter):
    name = 'Runahead Cycles'
    fname = 're_cycles'
    description = 'Histogram of how many cycles were spent in runahead periods'

    def plot(self) -> None:
        data = self.data['system']['cpu']['runaheadCycles']

        ys = np.array([
            data['0-127']['values'][0],
            data['128-255']['values'][0],
            data['256-383']['values'][0],
            data['384-511']['values'][0],
            data['512-639']['values'][0],
            data['640-767']['values'][0],
            data['768-895']['values'][0],
        ])
        xs = np.arange(len(ys))

        plt.bar(
            xs, ys,
            tick_label=[
                data['0-127']['bucket'],
                data['128-255']['bucket'],
                data['256-383']['bucket'],
                data['384-511']['bucket'],
                data['512-639']['bucket'],
                data['640-767']['bucket'],
                data['768-895']['bucket'],
            ],
        )
