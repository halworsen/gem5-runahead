#include "learning/part2/howdy_object.hh"
#include "base/trace.hh"
#include "debug/Howdy.hh"
#include <iostream>

namespace gem5
{

HowdyObject::HowdyObject(const HowdyObjectParams &params) :
    SimObject(params), event([this]{processEvent();}, name()),
    eventLatency(params.event_latency),
    eventRepeats(params.fire_amount),
    takeCare(params.take_care)
{
    DPRINTF(Howdy, "howdy instantiation with name %s\n", name());
    panic_if(!takeCare, "HowdyObject must have a non-null TakeCareObject!");
}

void
HowdyObject::processEvent()
{
    DPRINTF(Howdy, "event fired @ t%d\n", curTick());

    if (eventRepeats-- > 0) {
        schedule(event, curTick() + eventLatency);
    } else {
        takeCare->takeCare(name());
    }
}

void
HowdyObject::startup()
{
    DPRINTF(
        Howdy,
        "howdy startup. scheduling %d events for execution with latency %d\n",
        eventRepeats, eventLatency
    );


    eventRepeats--;
    schedule(event, eventLatency);
}

}
