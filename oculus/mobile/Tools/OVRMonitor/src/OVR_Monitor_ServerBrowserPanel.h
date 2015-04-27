#ifndef OVRSERVERBROWSERPANEL_H
#define OVRSERVERBROWSERPANEL_H

#include <wx/wx.h>

namespace OVR
{
namespace Monitor
{

    class SessionData;
    class ScannerThread;
    class CollectionThread;
    class ATraceThread;

    class ServerBrowserPanel : public ::wxPanel
    {
        DECLARE_EVENT_TABLE()
        public:
            ServerBrowserPanel(::wxWindow &parent, SessionData &sessionData);
            virtual ~ServerBrowserPanel(void);

            void openCaptureFile(wxString path);

        private:
            void onHostUpdateTimer(wxTimerEvent &event);
            void onServerList(wxCommandEvent &event);
            void onConnectButton(wxCommandEvent &event);

        private:
            enum
            {
                ID_HostListUpdateTimer = wxID_HIGHEST + 100, // +100 to avoid collisions with Frame
                ID_ServerList,
                ID_ConnectButton,
            };

        private:
            SessionData      &m_sessionData;

            wxListBox        *m_serverList;
            wxButton         *m_connectButton;

            wxCheckBox       *m_reconnectCheckbox;
            wxCheckBox       *m_cpuZonesCheckBox;
            wxCheckBox       *m_gpuZonesCheckBox;
            wxCheckBox       *m_cpuClocksCheckBox;
            wxCheckBox       *m_gpuClocksCheckBox;
            wxCheckBox       *m_thermalCheckBox;
            wxCheckBox       *m_framebufferCheckbox;
            wxCheckBox       *m_loggingCheckbox;
            wxCheckBox       *m_systraceCheckBox;

            ScannerThread    *m_scanner;
            CollectionThread *m_collector;
            ATraceThread     *m_atrace;
    };

} // namespace Monitor
} // namespace OVR

#endif
