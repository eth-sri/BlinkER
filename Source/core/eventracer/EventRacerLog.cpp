#include "config.h"
#include "EventRacerLog.h"
#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/loader/FrameLoaderClient.h"

namespace WebCore {

WTF::PassRefPtr<EventRacerLog> EventRacerLog::create()
{
    return WTF::adoptRef(new EventRacerLog());
}

EventRacerLog::EventRacerLog()
{
}

void EventRacerLog::documentCreated(Document *doc)
{
    LocalFrame *frame = doc->frame();
    if (frame)
        frame->loader().client()->dispatchDidBeginEventRacerDocument();
}
        
void EventRacerLog::documentDestroyed(Document *doc)
{
    LocalFrame *frame = doc->frame();
    if (frame)
        frame->loader().client()->dispatchDidEndEventRacerDocument();
}

} // end namespace WebCore
