#import <Cocoa/Cocoa.h>

#include "OVR_Monitor_Frame.h"

namespace OVR
{
namespace Monitor
{

    void Frame::EnableMacFullscreenSupport(bool enable)
    {
        NSWindow *macWindow = MacGetTopLevelWindowRef();
        if([macWindow respondsToSelector:@selector(setCollectionBehavior:)])
        {
            NSUInteger collectionBehavior = [macWindow collectionBehavior];
            if(enable)
            {
                collectionBehavior |= NSWindowCollectionBehaviorFullScreenPrimary;
            }
            else
            {
                collectionBehavior &= ~NSWindowCollectionBehaviorFullScreenPrimary;
            }
            [macWindow setCollectionBehavior: collectionBehavior];
        }
    }

} // namespace Monitor
} // namespace OVR
