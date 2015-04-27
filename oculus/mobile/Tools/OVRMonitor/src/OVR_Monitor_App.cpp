#include <wx/wx.h>
#include <wx/socket.h>

#include "OVR_Monitor_Frame.h"

class OVRMonitorApp: public wxApp
{
    virtual bool OnInit(void)
    {
        wxSocketBase::Initialize();
        wxImage::AddHandler(new wxPNGHandler);
        wxImage::AddHandler(new wxJPEGHandler);
        OVR::Monitor::Frame *frame = new OVR::Monitor::Frame();
        frame->Show(true);
        return true;
    }
};

wxDECLARE_APP(OVRMonitorApp);
wxIMPLEMENT_APP(OVRMonitorApp);
