#ifndef __GEM5_EXT_LEARNING_TAKECARE_OBJECT_HH__
#define __GEM5_EXT_LEARNING_TAKECARE_OBJECT_HH__

#include <string>
#include "params/TakeCareObject.hh"
#include "sim/sim_object.hh"

namespace gem5
{

class TakeCareObject : public SimObject
{
private:
    void processEvent();
    EventWrapper<TakeCareObject, &TakeCareObject::processEvent> event;

    // params
    float bandwidth; // ticks/byte
    int bufSize;
    std::string messageFormat;

    std::string message;
    int bufUsed;
    char* tcbuf;

public:
        TakeCareObject(const TakeCareObjectParams &p);
        ~TakeCareObject();

        void takeCare(std::string name);
};

}

#endif
