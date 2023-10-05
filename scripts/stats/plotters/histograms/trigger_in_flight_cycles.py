from ..plotter import Plotter
import matplotlib.pyplot as plt
import numpy as np

class TriggerInFlightCycles(Plotter):
    name = 'LLL cycles in flight on runahead trigger'
    fname = 'lll_in_flight_cycles'
    description = 'Histogram of how many cycles LLLs were in-flight for when they triggered RE'

    def plot(self) -> None:
        data = self.data[0]['system']['cpu']['triggerLLLinFlightCycles']

        ys = np.array([
            data['0-15']['values'][0],
            data['16-31']['values'][0],
            data['32-47']['values'][0],
            data['48-63']['values'][0],
            data['64-79']['values'][0],
            data['80-95']['values'][0],
            data['96-111']['values'][0],
            data['112-127']['values'][0],
        ])
        xs = np.arange(len(ys))

        plt.bar(xs, ys)
        plt.xticks(
            ticks=xs,
            labels=[
                data['0-15']['bucket'],
                data['16-31']['bucket'],
                data['32-47']['bucket'],
                data['48-63']['bucket'],
                data['64-79']['bucket'],
                data['80-95']['bucket'],
                data['96-111']['bucket'],
                data['112-127']['bucket'],
            ],
            rotation=45,
        )