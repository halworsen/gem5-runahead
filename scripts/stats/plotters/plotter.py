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

    log_dir = '/cluster/home/markuswh/gem5-runahead/spec2017/logs'

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

    def load_data(self) -> None:
        '''
        Load any data required to produce the plot
        '''
        pass

    def plot(self) -> None:
        '''
        Generate a plot using the stored data
        '''
        pass
