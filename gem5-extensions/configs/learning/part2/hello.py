import m5
from m5.objects import *

root = Root(full_system=False)
root.unreasonably_long_name_for_a_howdy = HowdyObject(event_latency='2us')
root.unreasonably_long_name_for_a_howdy.fire_amount = 10

take_care = TakeCareObject(bandwidth='1MB/s')
take_care.buf_message = 'take care, %s'
root.unreasonably_long_name_for_a_howdy.take_care = take_care

m5.instantiate()
exit_event = m5.simulate()

print(f'Sim exited. t{m5.curTick()} - {exit_event.getCause()}')
