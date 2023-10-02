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
        data = self.data['system']['cpu']['instsBetweenRunahead']

        ys = np.array([
            data['0-999']['values'][0],
            data['1000-1999']['values'][0],
            data['2000-2999']['values'][0],
            data['3000-3999']['values'][0],
            data['4000-4999']['values'][0],
            data['5000-5999']['values'][0],
            data['6000-6999']['values'][0],
            data['7000-7999']['values'][0],
            data['8000-8999']['values'][0],
            data['9000-9999']['values'][0],
        ])
        xs = np.arange(len(ys))

        plt.bar(xs, ys)
        plt.xticks(
            ticks=xs,
            labels=[
                data['0-999']['bucket'],
                data['1000-1999']['bucket'],
                data['2000-2999']['bucket'],
                data['3000-3999']['bucket'],
                data['4000-4999']['bucket'],
                data['5000-5999']['bucket'],
                data['6000-6999']['bucket'],
                data['7000-7999']['bucket'],
                data['8000-8999']['bucket'],
                data['9000-9999']['bucket'],
            ],
            rotation=45,
        )
