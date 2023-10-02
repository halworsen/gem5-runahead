'''
Base class for plot producers
'''

class Plotter:
    name = 'base plotter class'
    ''' Pretty name for the figure '''
    fname = 'baseplotterclass.jpg'
    ''' Filename of the output plot '''
    description = 'base plotter class'
    ''' Description of what kind of plot this plotter makes '''

    def __init__(self, data: dict):
        self.data = data

    def plot(self) -> None:
        '''
        Generate a plot using the stored data
        '''
        pass
