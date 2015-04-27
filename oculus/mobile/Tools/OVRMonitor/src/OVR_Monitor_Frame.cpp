#include "OVR_Monitor_Frame.h"
#include "OVR_Monitor_ServerBrowserPanel.h"
#include "OVR_Monitor_FrameViewerPanel.h"
#include "OVR_Monitor_ProfilerPanel.h"
#include "OVR_Monitor_LogPanel.h"
#include "OVR_Monitor_SettingsPanel.h"
#include "OVR_Monitor_Settings.h"

#include "icons/connect.png.h"
#include "icons/load.png.h"
#include "icons/camera.png.h"
#include "icons/chart.png.h"
#include "icons/log.png.h"
#include "icons/settings.png.h"

namespace OVR
{
namespace Monitor
{

    wxDEFINE_EVENT(NewDataEvent,     wxCommandEvent);
    wxDEFINE_EVENT(OnNewLog,         NewLogEvent);
    wxDEFINE_EVENT(OnConnectionOpen, wxCommandEvent);

    BEGIN_EVENT_TABLE(Frame, wxFrame)
        EVT_MENU(   ID_ServerBrowserButton,     Frame::onClickServerBrowser)
        EVT_MENU(   ID_OpenCaptureFile,         Frame::onClickOpenCaptureFile)
        EVT_MENU(   ID_FrameViewerButton,       Frame::onClickFrameViewer)
        EVT_MENU(   ID_ProfilerButton,          Frame::onClickProfiler)
        EVT_MENU(   ID_LogButton,               Frame::onClickLog)
        EVT_MENU(   ID_SettingsButton,          Frame::onClickSettings)
        EVT_COMMAND(wxID_ANY, NewDataEvent,     Frame::onDataAvailable)
        EVT_NEWLOG( wxID_ANY,                   Frame::onLogMessage)
        EVT_COMMAND(wxID_ANY, OnConnectionOpen, Frame::onClickProfiler)
    END_EVENT_TABLE()

    Frame::Frame(void) :
        wxFrame(0, wxID_ANY, wxT("Oculus Remote Monitor"), wxDefaultPosition, wxSize(1280,720)),
        m_sessionData(*this)
    {
        m_serverBrowserPanel = NULL;
        m_frameViewerPanel   = NULL;
        m_profilerPanel      = NULL;
        m_logPanel           = NULL;
        m_settingsPanel      = NULL;

        SetMinClientSize(wxSize(800,600));

    #if defined(__WXMAC__)
        EnableMacFullscreenSupport(true);
    #endif

        wxToolBar *toolbar = CreateToolBar();
        toolbar->SetMargins(5, 5);
        toolbar->SetToolBitmapSize(wxSize(16, 16));
        toolbar->AddTool(ID_ServerBrowserButton, "Connect",  wxBitmap::NewFromPNGData(g_connect_png,  sizeof(g_connect_png)),  wxNullBitmap, wxITEM_NORMAL, "Server Browser",    "Locate and connect to a remote LibOVR session");
        toolbar->AddTool(ID_OpenCaptureFile,     "Load",     wxBitmap::NewFromPNGData(g_load_png,     sizeof(g_load_png)),     wxNullBitmap, wxITEM_NORMAL, "Open Capture File", "Open a capture stream saved on disk");
        toolbar->AddTool(ID_FrameViewerButton,   "Frame",    wxBitmap::NewFromPNGData(g_camera_png,   sizeof(g_camera_png)),   wxNullBitmap, wxITEM_NORMAL, "Viewport Stream",   "View remote framebuffer in real-time");
        toolbar->AddTool(ID_ProfilerButton,      "Chart",    wxBitmap::NewFromPNGData(g_chart_png,    sizeof(g_chart_png)),    wxNullBitmap, wxITEM_NORMAL, "Performance Data",  "View recorded performance data");
        toolbar->AddTool(ID_LogButton,           "Log",      wxBitmap::NewFromPNGData(g_log_png,      sizeof(g_log_png)),      wxNullBitmap, wxITEM_NORMAL, "Log",               "View message log");
        toolbar->AddStretchableSpace();
        toolbar->AddTool(ID_SettingsButton,      "Config",   wxBitmap::NewFromPNGData(g_settings_png, sizeof(g_settings_png)), wxNullBitmap, wxITEM_NORMAL, "Settings",          "Adjust Oculus Remote Monitor settings");
        toolbar->Realize();

        (m_serverBrowserPanel = new ServerBrowserPanel(*this, m_sessionData))->Show();
        (m_frameViewerPanel   = new FrameViewerPanel(*this, m_sessionData))->Hide();
        (m_profilerPanel      = new ProfilerPanel(*this, m_sessionData))->Hide();
        (m_logPanel           = new LogPanel(*this))->Hide();
        (m_settingsPanel      = new SettingsPanel(*this))->Hide();

        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(m_serverBrowserPanel, 1, wxEXPAND);
        sizer->Add(m_frameViewerPanel,   1, wxEXPAND);
        sizer->Add(m_profilerPanel,      1, wxEXPAND);
        sizer->Add(m_logPanel,           1, wxEXPAND);
        sizer->Add(m_settingsPanel,      1, wxEXPAND);
        SetSizer(sizer);
    }

    Frame::~Frame(void)
    {
        // We delete in a specific order... mostly because wx will tear down in some unknown order
        // and things like the server browser create threads that touch other objects...
        // TODO: we should probably do this in some sort of OnClose call, not in the destructor.
        delete m_serverBrowserPanel;
        delete m_frameViewerPanel;
        delete m_profilerPanel;
        delete m_settingsPanel;
    }

    void Frame::onClickServerBrowser(wxCommandEvent &event)
    {
        m_serverBrowserPanel->Show();
        m_frameViewerPanel->Hide();
        m_profilerPanel->Hide();
        m_logPanel->Hide();
        m_settingsPanel->Hide();
        Layout();
        m_serverBrowserPanel->SetFocus();
    }

    void Frame::onClickOpenCaptureFile(wxCommandEvent &event)
    {
        const wxString defaultDir = Settings().GetRecordingsDir().GetPath();
        wxFileDialog openFileDialog(this, "Capture File", defaultDir, wxEmptyString, "Capture File (*.dat)|*.dat", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if(openFileDialog.ShowModal() != wxID_CANCEL)
        {
            m_serverBrowserPanel->openCaptureFile(openFileDialog.GetPath());
        }
    }

    void Frame::onClickFrameViewer(wxCommandEvent &event)
    {
        m_serverBrowserPanel->Hide();
        m_frameViewerPanel->Show();
        m_profilerPanel->Hide();
        m_logPanel->Hide();
        m_settingsPanel->Hide();
        Layout();
        m_frameViewerPanel->SetFocus();
    }

    void Frame::onClickProfiler(wxCommandEvent &event)
    {
        m_serverBrowserPanel->Hide();
        m_frameViewerPanel->Hide();
        m_profilerPanel->Show();
        m_logPanel->Hide();
        m_settingsPanel->Hide();
        Layout();
        m_profilerPanel->SetFocus();
    }

    void Frame::onClickLog(wxCommandEvent &event)
    {
        m_serverBrowserPanel->Hide();
        m_frameViewerPanel->Hide();
        m_profilerPanel->Hide();
        m_logPanel->Show();
        m_settingsPanel->Hide();
        Layout();
        m_settingsPanel->SetFocus();
    }

    void Frame::onClickSettings(wxCommandEvent &event)
    {
        m_serverBrowserPanel->Hide();
        m_frameViewerPanel->Hide();
        m_profilerPanel->Hide();
        m_logPanel->Hide();
        m_settingsPanel->Show();
        Layout();
        m_settingsPanel->SetFocus();
    }

    void Frame::onDataAvailable(wxCommandEvent &event)
    {
        if(m_frameViewerPanel->IsShown())
            m_frameViewerPanel->Refresh(false);
        if(m_profilerPanel->IsShown())
            m_profilerPanel->Refresh(false);
    }

    void Frame::onLogMessage(NewLogEvent &event)
    {
        wxString threadName = event.threadName;
        if(threadName.empty())
            threadName = wxString::Format("%u", event.threadID);
        m_logPanel->pushLog(threadName, event.priority, event.timeInSeconds, event.message);
    }

} // namespace Monitor
} // namespace OVR