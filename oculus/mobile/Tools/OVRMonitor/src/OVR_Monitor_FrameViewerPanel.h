#ifndef OVRFRAMEVIEWERPANEL_H
#define OVRFRAMEVIEWERPANEL_H

#include <wx/wx.h>

namespace OVR
{
namespace Monitor
{

    class SessionData;

    class FrameViewerPanel : public ::wxPanel
    {
        DECLARE_EVENT_TABLE()
        public:
            FrameViewerPanel(wxWindow &parent, SessionData &sessionData);
            virtual ~FrameViewerPanel(void);

        private:
    };

} // namespace Monitor
} // namespace OVR

#endif
