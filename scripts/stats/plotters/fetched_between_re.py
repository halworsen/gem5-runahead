'''
Histogram showing instructions fetched between runahead periods
'''

from ..plotter import Plotter

import matplotlib.pyplot as plt
import numpy as np

class InterimInsts(Plotter):
    name = 'interim_insts'
    fname = 'fetched_between_re'
    description = 'Histogram of instructions fetched in the interim between runahead periods'

    def plot(self) -> None:
        data = self.data[0]['system']['cpu']['instsBetweenRunahead']

        ys = np.array([
            data['0-99']['values'][0],
            data['100-199']['values'][0],
            data['200-299']['values'][0],
            data['300-399']['values'][0],
            data['400-499']['values'][0],
            data['500-599']['values'][0],
            data['600-699']['values'][0],
            data['700-799']['values'][0],
            data['800-899']['values'][0],
            data['900-999']['values'][0],
            data['1000-1099']['values'][0],
            data['1100-1199']['values'][0],
            data['1200-1299']['values'][0],
            data['1300-1399']['values'][0],
            data['1400-1499']['values'][0],
            data['1500-1599']['values'][0],
        ])
        xs = np.arange(len(ys))

        plt.bar(xs, ys)
        plt.xticks(
            ticks=xs,
            labels=[
                data['0-99']['bucket'],
                data['100-199']['bucket'],
                data['200-299']['bucket'],
                data['300-399']['bucket'],
                data['400-499']['bucket'],
                data['500-599']['bucket'],
                data['600-699']['bucket'],
                data['700-799']['bucket'],
                data['800-899']['bucket'],
                data['900-999']['bucket'],
                data['1000-1099']['bucket'],
                data['1100-1199']['bucket'],
                data['1200-1299']['bucket'],
                data['1300-1399']['bucket'],
                data['1400-1499']['bucket'],
                data['1500-1599']['bucket'],
            ],
            rotation=45,
        )
