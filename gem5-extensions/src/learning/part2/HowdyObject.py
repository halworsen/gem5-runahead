from m5.params import *
from m5.SimObject import SimObject


class HowdyObject(SimObject):
    type = 'HowdyObject'
    cxx_header = 'learning/part2/howdy_object.hh'
    cxx_class = 'gem5::HowdyObject'

    event_latency = Param.Latency('Time to wait before firing the event')
    fire_amount = Param.Int(1, 'Amount of times to fire the event')

    take_care = Param.TakeCareObject('A take care object')
