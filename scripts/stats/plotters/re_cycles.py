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
        data = self.data[0]['system']['cpu']['runaheadCycles']

        ys = np.array([
            data['0-49']['values'][0],
            data['50-99']['values'][0],
            data['100-149']['values'][0],
            data['150-199']['values'][0],
            data['200-249']['values'][0],
            data['250-299']['values'][0],
            data['300-349']['values'][0],
            data['350-399']['values'][0],
            data['400-449']['values'][0],
            data['450-499']['values'][0],
            data['500-549']['values'][0],
            data['550-599']['values'][0],
            data['600-649']['values'][0],
            data['650-699']['values'][0],
            data['700-749']['values'][0],
            data['750-799']['values'][0],
            data['800-849']['values'][0],
            data['850-899']['values'][0],
            data['900-949']['values'][0],
            data['950-999']['values'][0],
            data['1000']['values'][0],
        ])
        xs = np.arange(len(ys))

        plt.bar(xs, ys)
        plt.xticks(
            ticks=xs,
            labels=[
                data['0-49']['bucket'],
                data['50-99']['bucket'],
                data['100-149']['bucket'],
                data['150-199']['bucket'],
                data['200-249']['bucket'],
                data['250-299']['bucket'],
                data['300-349']['bucket'],
                data['350-399']['bucket'],
                data['400-449']['bucket'],
                data['450-499']['bucket'],
                data['500-549']['bucket'],
                data['550-599']['bucket'],
                data['600-649']['bucket'],
                data['650-699']['bucket'],
                data['700-749']['bucket'],
                data['750-799']['bucket'],
                data['800-849']['bucket'],
                data['850-899']['bucket'],
                data['900-949']['bucket'],
                data['950-999']['bucket'],
                data['1000']['bucket'],
            ],
            rotation=45,
        )