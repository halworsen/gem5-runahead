#ifndef __GEM5_EXT_LEARNING_HOWDY_OBJECT_HH__
#define __GEM5_EXT_LEARNING_HOWDY_OBJECT_HH__

#include "learning/part2/take_care_object.hh"
#include "params/HowdyObject.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class HowdyObject : public SimObject
{
private:
    void processEvent();
    EventFunctionWrapper event;
    const Tick eventLatency;
    int eventRepeats;

    TakeCareObject* takeCare;

public:
        HowdyObject(const HowdyObjectParams &p);

        void startup();
};

}

#endif
