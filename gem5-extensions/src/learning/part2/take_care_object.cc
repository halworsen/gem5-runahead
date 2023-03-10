#include "learning/part2/take_care_object.hh"
#include "base/trace.hh"
#include "debug/Howdy.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

TakeCareObject::TakeCareObject(const TakeCareObjectParams &params) :
    SimObject(params), event(*this),
    bandwidth(params.bandwidth),
    bufSize(params.buf_size),
    messageFormat(params.buf_message),
    bufUsed(0),
    tcbuf(nullptr)
{
    tcbuf = new char[bufSize];
    DPRINTF(Howdy, "instantiation with name %s\n", name());
}

TakeCareObject::~TakeCareObject()
{
    delete[] tcbuf;
}

void
TakeCareObject::processEvent()
{
    DPRINTF(Howdy, "filling buffer with message once\n");

    assert(message.length() > 0);
    int bufStart = bufUsed;
    for (int i = 0; i < message.length() && bufUsed < (bufSize - 1); i++) {
        tcbuf[bufUsed++] = message.at(i);
    }

    int bytesCopied = bufUsed - bufStart;
    int delay = bandwidth * bytesCopied;
    if (bufUsed < bufSize - 1) {
        DPRINTF(Howdy, "Scheduling new event in %d ticks\n", delay);
        schedule(event, curTick() + delay);
    } else {
        DPRINTF(Howdy, "Done copying, exiting in %d ticks\n", delay);
        // exit with message = tcbuf, exit code 0 in curTick() + delay ticks
        exitSimLoop(tcbuf, 0, curTick() + delay);
    }
}

void
TakeCareObject::takeCare(std::string name)
{
    // hacky but meh. -2 because %s is 2 characters long
    size_t buf_length = (messageFormat.length() - 2) + name.length();
    char* formatted_buf = new char[buf_length];
    sprintf(formatted_buf, messageFormat.c_str(), name.c_str());

    message = std::string(formatted_buf);
    schedule(event, curTick());
}

}
