'''
Load-to-use histogram
'''

from ..plotter import Plotter
import numpy as np
import matplotlib.pyplot as plt

class LoadToUse(Plotter):
    name = 'Load to use'
    fname = 'load_to_use'
    description = 'Distrubtion chart of load-to-use cycles'

    def plot(self) -> None:
        normal_data = self.data[1]['system']['cpu']['lsq0']['loadToUse']
        re_data = self.data[0]['system']['cpu']['lsq0']['realLoadToUse']

        ys_normal = np.array([
            normal_data['10-19']['values'][1],
            normal_data['20-29']['values'][1],
            normal_data['30-39']['values'][1],
            normal_data['40-49']['values'][1],
            normal_data['50-59']['values'][1],
            normal_data['60-69']['values'][1],
            normal_data['70-79']['values'][1],
            normal_data['80-89']['values'][1],
            normal_data['90-99']['values'][1],
            normal_data['100-109']['values'][1],
            normal_data['110-119']['values'][1],
            normal_data['120-129']['values'][1],
            normal_data['130-139']['values'][1],
            normal_data['140-149']['values'][1],
            normal_data['150-159']['values'][1],
            normal_data['160-169']['values'][1],
            normal_data['170-179']['values'][1],
            normal_data['180-189']['values'][1],
            normal_data['190-199']['values'][1],
            normal_data['200-209']['values'][1],
            normal_data['210-219']['values'][1],
            normal_data['220-229']['values'][1],
            normal_data['230-239']['values'][1],
            normal_data['240-249']['values'][1],
            normal_data['250-259']['values'][1],
            normal_data['260-269']['values'][1],
            normal_data['270-279']['values'][1],
            normal_data['280-289']['values'][1],
            normal_data['290-299']['values'][1],
        ])

        ys_runahead = np.array([
            re_data['10-19']['values'][1],
            re_data['20-29']['values'][1],
            re_data['30-39']['values'][1],
            re_data['40-49']['values'][1],
            re_data['50-59']['values'][1],
            re_data['60-69']['values'][1],
            re_data['70-79']['values'][1],
            re_data['80-89']['values'][1],
            re_data['90-99']['values'][1],
            re_data['100-109']['values'][1],
            re_data['110-119']['values'][1],
            re_data['120-129']['values'][1],
            re_data['130-139']['values'][1],
            re_data['140-149']['values'][1],
            re_data['150-159']['values'][1],
            re_data['160-169']['values'][1],
            re_data['170-179']['values'][1],
            re_data['180-189']['values'][1],
            re_data['190-199']['values'][1],
            re_data['200-209']['values'][1],
            re_data['210-219']['values'][1],
            re_data['220-229']['values'][1],
            re_data['230-239']['values'][1],
            re_data['240-249']['values'][1],
            re_data['250-259']['values'][1],
            re_data['260-269']['values'][1],
            re_data['270-279']['values'][1],
            re_data['280-289']['values'][1],
            re_data['290-299']['values'][1],
        ])

        xs = np.arange(len(ys_normal))
        plt.bar(xs - 0.125, ys_normal, color='red', width=0.25, label='Normal')
        plt.bar(xs + 0.125, ys_runahead, color='green', width=0.25, label='Runahead')
        plt.legend(['Normal', 'Runahead'])
        plt.xticks(
            ticks=xs,
            labels=[
                normal_data['10-19']['bucket'],
                normal_data['20-29']['bucket'],
                normal_data['30-39']['bucket'],
                normal_data['40-49']['bucket'],
                normal_data['50-59']['bucket'],
                normal_data['60-69']['bucket'],
                normal_data['70-79']['bucket'],
                normal_data['80-89']['bucket'],
                normal_data['90-99']['bucket'],
                normal_data['100-109']['bucket'],
                normal_data['110-119']['bucket'],
                normal_data['120-129']['bucket'],
                normal_data['130-139']['bucket'],
                normal_data['140-149']['bucket'],
                normal_data['150-159']['bucket'],
                normal_data['160-169']['bucket'],
                normal_data['170-179']['bucket'],
                normal_data['180-189']['bucket'],
                normal_data['190-199']['bucket'],
                normal_data['200-209']['bucket'],
                normal_data['210-219']['bucket'],
                normal_data['220-229']['bucket'],
                normal_data['230-239']['bucket'],
                normal_data['240-249']['bucket'],
                normal_data['250-259']['bucket'],
                normal_data['260-269']['bucket'],
                normal_data['270-279']['bucket'],
                normal_data['280-289']['bucket'],
                normal_data['290-299']['bucket'],
            ],
            rotation=45,
        )
