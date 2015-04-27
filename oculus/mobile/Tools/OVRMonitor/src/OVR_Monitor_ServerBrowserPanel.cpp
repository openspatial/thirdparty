#include "OVR_Monitor_ServerBrowserPanel.h"
#include "OVR_Monitor_CollectionThread.h"
#include "OVR_Monitor_ATrace.h"
#include "OVR_Monitor_Settings.h"
#include "OVR_Monitor_Frame.h"

#include <wx/statline.h>
#include <wx/socket.h>

namespace OVR
{
namespace Monitor
{

    class HostData
    {
        public:
            wxLongLong     lastSeenTime;
            wxString       ipaddress;
            wxString       packageName;
            unsigned short tcpPort;
    };

    class ScannerThread : public wxThread
    {
        public:
            ScannerThread(wxListBox &listbox) :
                wxThread(wxTHREAD_JOINABLE),
                m_listbox(listbox)
            {
                wxIPV4address addr;
                addr.AnyAddress();
                addr.Service(2020);

                m_socket = new wxDatagramSocket(addr, wxSOCKET_NONE);
            }

            virtual ~ScannerThread(void)
            {
                if(m_socket) m_socket->Destroy();
            }

            void CloseSocket(void)
            {
                if(m_socket) m_socket->Close();
            }

        private:
            virtual void *Entry(void)
            {
                while(m_socket && !TestDestroy())
                {
                    Capture::ZeroConfigPacket header = {0};
                    wxIPV4address addr;
                    m_socket->RecvFrom(addr, &header, sizeof(header));
                    if(m_socket->LastCount() == sizeof(header) && header.magicNumber == header.s_magicNumber)
                    {
                        const wxString connectionLabel = wxString::Format("%s:%d -- %s", addr.IPAddress().mb_str().data(), header.tcpPort, header.packageName);
                        wxMutexGuiEnter();
                        const int found = m_listbox.FindString(connectionLabel, true);
                        HostData *hostData = NULL;
                        if(found < 0)
                        {
                            // item not found... so lets add it.
                            hostData = new HostData;
                            m_listbox.Append(connectionLabel, hostData);
                        }
                        else
                        {
                            // item found... so we are just going to update an existing HostData
                            hostData = (HostData*)m_listbox.GetClientData(found);
                            wxASSERT(hostData);
                        }
                        if(hostData)
                        {
                            hostData->lastSeenTime = wxGetLocalTimeMillis();
                            hostData->ipaddress    = addr.IPAddress();
                            hostData->packageName  = header.packageName;
                            hostData->tcpPort      = header.tcpPort;
                        }
                        wxMutexGuiLeave();
                    }
                }
                return 0;
            }

        private:
            wxListBox        &m_listbox;
            wxDatagramSocket *m_socket;
    };

    /*********************
    * ServerBrowserPanel *
    *********************/

    BEGIN_EVENT_TABLE(ServerBrowserPanel, wxPanel)
        EVT_TIMER(  ID_HostListUpdateTimer,  ServerBrowserPanel::onHostUpdateTimer)
        EVT_LISTBOX(ID_ServerList,           ServerBrowserPanel::onServerList)
        EVT_BUTTON( ID_ConnectButton,        ServerBrowserPanel::onConnectButton)
    END_EVENT_TABLE()

    ServerBrowserPanel::ServerBrowserPanel(wxWindow &parent, SessionData &sessionData) :
        wxPanel(&parent),
        m_sessionData(sessionData)
    {
        m_serverList          = NULL;
        m_connectButton       = NULL;
        m_reconnectCheckbox   = NULL;
        m_cpuZonesCheckBox    = NULL;
        m_gpuZonesCheckBox    = NULL;
        m_cpuClocksCheckBox   = NULL;
        m_gpuClocksCheckBox   = NULL;
        m_thermalCheckBox     = NULL;
        m_framebufferCheckbox = NULL;
        m_loggingCheckbox     = NULL;
        m_systraceCheckBox    = NULL;
        m_scanner             = NULL;
        m_collector           = NULL;
        m_atrace              = NULL;

        SetLabel("Server Browser");

        m_serverList    = new wxListBox(this, ID_ServerList);
        m_connectButton = new wxButton(this, ID_ConnectButton, "Connect");

        wxBoxSizer *leftSizer = new wxBoxSizer(wxVERTICAL);
        leftSizer->Add(new wxStaticText(this, wxID_ANY, "Available Hosts"), 0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND,                  20);
        leftSizer->Add(m_serverList,                                        1, wxTOP | wxLEFT | wxRIGHT | wxEXPAND,                  20);
        leftSizer->Add(m_connectButton,                                     0, wxTOP | wxLEFT | wxRIGHT | wxBOTTOM | wxALIGN_RIGHT,  20);


        (m_reconnectCheckbox   = new wxCheckBox(this, wxID_ANY, "Auto Reconnect"))->SetValue(true);
        (m_cpuZonesCheckBox    = new wxCheckBox(this, wxID_ANY, "CPU Zones"))->SetValue(true);
        (m_gpuZonesCheckBox    = new wxCheckBox(this, wxID_ANY, "GPU Zones"))->SetValue(false);
        (m_cpuClocksCheckBox   = new wxCheckBox(this, wxID_ANY, "CPU Clocks"))->SetValue(true);
        (m_gpuClocksCheckBox   = new wxCheckBox(this, wxID_ANY, "GPU Clocks"))->SetValue(true);
        (m_thermalCheckBox     = new wxCheckBox(this, wxID_ANY, "Thermal Zones"))->SetValue(false);
        (m_framebufferCheckbox = new wxCheckBox(this, wxID_ANY, "Capture Frame Buffer"))->SetValue(true);
        (m_loggingCheckbox     = new wxCheckBox(this, wxID_ANY, "Logging"))->SetValue(true);
        (m_systraceCheckBox    = new wxCheckBox(this, wxID_ANY, "systrace"))->SetValue(false);

        // These features are currently not functional...
        m_gpuZonesCheckBox->Disable();
        m_systraceCheckBox->Disable();

        wxBoxSizer *rightSizer = new wxBoxSizer(wxVERTICAL);
        rightSizer->Add(new wxStaticText(this, wxID_ANY, "Session Settings"), 0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_reconnectCheckbox,                                  0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_cpuZonesCheckBox,                                   0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_gpuZonesCheckBox,                                   0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_cpuClocksCheckBox,                                  0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_gpuClocksCheckBox,                                  0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_thermalCheckBox,                                    0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_framebufferCheckbox,                                0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_loggingCheckbox,                                    0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);
        rightSizer->Add(m_systraceCheckBox,                                   0, wxTOP | wxLEFT | wxRIGHT | wxEXPAND, 20);


        wxBoxSizer *sizer = new wxBoxSizer(wxHORIZONTAL);
        sizer->Add(leftSizer, 1, wxEXPAND);
        sizer->Add(new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL), 0, wxTOP | wxBOTTOM | wxEXPAND, 10);
        sizer->Add(rightSizer, 1, wxEXPAND);

        SetSizer(sizer);

        m_scanner = new ScannerThread(*m_serverList);
        m_scanner->Create();
        m_scanner->Run();

        wxTimer *timer = new wxTimer(this, ID_HostListUpdateTimer);
        timer->Start(1000, false);
    }

    ServerBrowserPanel::~ServerBrowserPanel(void)
    {
        if(m_scanner)
        {
            m_scanner->CloseSocket();
            m_scanner->Delete();
            delete m_scanner;
        }
        if(m_collector)
        {
            m_collector->Close();
            m_collector->Delete();
            delete m_collector;
        }
        if(m_atrace)
        {
            m_atrace->Delete();
            delete m_atrace;
        }

        if(m_serverList)
        {
            for(unsigned int i=0; i<m_serverList->GetCount(); i++)
            {
                HostData *hostData = (HostData*)m_serverList->GetClientData(i);
                if(hostData) delete hostData;
            }
            m_serverList->Clear();
        }
    }

    void ServerBrowserPanel::openCaptureFile(wxString path)
    {
        // Kill any existing collectors...
        if(m_collector)
        {
            m_collector->Close();
            m_collector->Delete();
            delete m_collector;
            m_collector = NULL;
        }

        // Kill atrace...
        if(m_atrace)
        {
            m_atrace->Delete();
            delete m_atrace;
            m_atrace = NULL;
        }

        // Reset the session data and start fresh...
        m_sessionData.reset();

        // Start collector...
        m_collector = new FileCollectionThread(wxFileName(path), m_sessionData);
        m_collector->Create();
        m_collector->Run();

        // Send event to notify that a new connection just opened...
        wxPostEvent(GetParent(), wxCommandEvent(OnConnectionOpen));
    }

    void ServerBrowserPanel::onHostUpdateTimer(wxTimerEvent &event)
    {
        if(m_serverList)
        {
            const wxLongLong currentTime = wxGetLocalTimeMillis();
            for(unsigned int i=0; i<m_serverList->GetCount(); i++)
            {
                HostData *hostData = (HostData*)m_serverList->GetClientData(i);
                //wxASSERT(hostData);
                if(!hostData || (currentTime-hostData->lastSeenTime) > 2000)
                {
                    // if we have not seen the host in 2 seconds, remove it.
                    if(hostData) delete hostData;
                    m_serverList->Delete(i);
                    i--;
                }
            }
        }
    }

    void ServerBrowserPanel::onServerList(wxCommandEvent &event)
    {
    }

    void ServerBrowserPanel::onConnectButton(wxCommandEvent &event)
    {
        const int selection = m_serverList->GetSelection();
        if(selection != wxNOT_FOUND)
        {
            HostData *hostData = (HostData*)m_serverList->GetClientData(selection);
            if(hostData)
            {
                if(m_collector)
                {
                    m_collector->Close();
                    m_collector->Delete();
                    delete m_collector;
                    m_collector = NULL;
                }
                if(m_atrace)
                {
                    m_atrace->Delete();
                    delete m_atrace;
                    m_atrace = NULL;
                }

                // Remote address to connect to...
                wxIPV4address addr;
                addr.Hostname(hostData->ipaddress);
                addr.Service(hostData->tcpPort);

                // File to write stream into...
                const wxDateTime datetime   = wxDateTime::UNow();
                const wxString   datestring = wxString::Format("%04u%02u%02u-%02u%02u%02u", datetime.GetYear(), (datetime.GetMonth()-wxDateTime::Jan)+1, datetime.GetDay(), datetime.GetHour(), datetime.GetMinute(), datetime.GetSecond());
                const wxString   streamName = hostData->packageName + wxString("-") + datestring + wxString(".dat");
                const wxFileName streamPath = wxFileName(Settings().GetRecordingsDir().GetPath(), streamName);

                // Reset the session data and start fresh...
                m_sessionData.reset();

                // Build collection flags...
                Capture::UInt32 connectionFlags = 0;
                if(m_cpuZonesCheckBox->IsChecked())    connectionFlags |= Capture::Enable_CPU_Zones;
                if(m_gpuZonesCheckBox->IsChecked())    connectionFlags |= Capture::Enable_GPU_Zones;
                if(m_cpuClocksCheckBox->IsChecked())   connectionFlags |= Capture::Enable_CPU_Clocks;
                if(m_gpuClocksCheckBox->IsChecked())   connectionFlags |= Capture::Enable_GPU_Clocks;
                if(m_thermalCheckBox->IsChecked())     connectionFlags |= Capture::Enable_Thermal_Sensors;
                if(m_framebufferCheckbox->IsChecked()) connectionFlags |= Capture::Enable_FrameBuffer_Capture;
                if(m_loggingCheckbox->IsChecked())     connectionFlags |= Capture::Enable_Logging;

                // Start collector...
                m_collector = new SocketCollectionThread(addr, streamPath, m_sessionData, connectionFlags);
                m_collector->Create();
                m_collector->Run();

                // Optionally start ATrace...
                if(m_systraceCheckBox->IsChecked())
                {
                    m_atrace = new ATraceThread(m_sessionData);
                    m_atrace->Create();
                    m_atrace->Run();
                }

                // Send event to notify that a new connection just opened...
                wxPostEvent(GetParent(), wxCommandEvent(OnConnectionOpen));
            }
        }
    }

} // namespace Monitor
} // namespace OVR
