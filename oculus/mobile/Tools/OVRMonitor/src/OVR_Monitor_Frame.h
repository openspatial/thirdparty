#ifndef OVRMONITORFRAME_H
#define OVRMONITORFRAME_H

#include <wx/wx.h>

#include "OVR_Monitor_SessionData.h"

namespace OVR
{
namespace Monitor
{

    class ServerBrowserPanel;
    class FrameViewerPanel;
    class ProfilerPanel;
    class LogPanel;
    class SettingsPanel;

    class NewLogEvent;

    wxDECLARE_EVENT(NewDataEvent,     wxCommandEvent);
    wxDECLARE_EVENT(OnNewLog,         NewLogEvent);
    wxDECLARE_EVENT(OnConnectionOpen, wxCommandEvent);

    class NewLogEvent : public wxCommandEvent
    {
        public:
            NewLogEvent(wxEventType commandType=OnNewLog, int id=0) :
                wxCommandEvent(commandType,id)
            {
            }

            NewLogEvent(const NewLogEvent &event) :
                wxCommandEvent(event)
            {
                threadID      = event.threadID;
                threadName    = event.threadName;
                priority      = event.priority;
                timeInSeconds = event.timeInSeconds;
                message       = event.message;
            }

            wxEvent *Clone(void) const
            {
                return new NewLogEvent(*this);
            }

        public:
            Capture::UInt32      threadID;
            wxString             threadName;
            Capture::LogPriority priority;
            double               timeInSeconds;
            wxString             message;
    };

    typedef void (wxEvtHandler::*NewLogEventFunction)(NewLogEvent &);
 
    #define NewLogEventHandler(func) wxEVENT_HANDLER_CAST(NewLogEventFunction, func)                    
    #define EVT_NEWLOG(id, func)     wx__DECLARE_EVT1(OnNewLog, id, NewLogEventHandler(func))


    class Frame : public wxFrame
    {
        DECLARE_EVENT_TABLE()
        public:
            Frame(void);
            virtual ~Frame(void);

        private:
        #if defined(__WXMAC__)
            void EnableMacFullscreenSupport(bool enable);
        #endif

            void onClickServerBrowser(wxCommandEvent &event);
            void onClickOpenCaptureFile(wxCommandEvent &event);
            void onClickFrameViewer(wxCommandEvent &event);
            void onClickProfiler(wxCommandEvent &event);
            void onClickLog(wxCommandEvent &event);
            void onClickSettings(wxCommandEvent &event);
            void onDataAvailable(wxCommandEvent &event);
            void onLogMessage(NewLogEvent &event);

        private:
            enum
            {
                ID_ServerBrowserButton = wxID_HIGHEST+1,
                ID_OpenCaptureFile,
                ID_FrameViewerButton,
                ID_ProfilerButton,
                ID_LogButton,
                ID_SettingsButton
            };

        private:
            SessionData         m_sessionData;

            ServerBrowserPanel *m_serverBrowserPanel;
            FrameViewerPanel   *m_frameViewerPanel;
            ProfilerPanel      *m_profilerPanel;
            LogPanel           *m_logPanel;
            SettingsPanel      *m_settingsPanel;
    };

} // namespace Monitor
} // namespace OVR

#endif
