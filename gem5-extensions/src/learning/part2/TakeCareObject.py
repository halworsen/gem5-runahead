from m5.params import *
from m5.SimObject import SimObject


class TakeCareObject(SimObject):
    type = 'TakeCareObject'
    cxx_header = 'learning/part2/take_care_object.hh'
    cxx_class = 'gem5::TakeCareObject'

    buf_message = Param.String('take care now, %s\n',
                               'The message to put into the buffer. %s will be substituted.')
    buf_size = Param.MemorySize('1kB', 'Size of the buffer to fill with messages')
    # converted to ticks/byte
    bandwidth = Param.MemoryBandwidth('100MB/s', 'Bandwidth to use when filling the buffer')
