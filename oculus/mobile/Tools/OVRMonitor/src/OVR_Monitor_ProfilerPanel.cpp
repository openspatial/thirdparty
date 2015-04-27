#include "OVR_Monitor_ProfilerPanel.h"
#include "OVR_Monitor_SessionData.h"

#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/dcbuffer.h>
#include <wx/sizer.h>
#include <wx/dcsvg.h>

namespace OVR
{
namespace Monitor
{

    BEGIN_EVENT_TABLE(ProfilerPanel, wxPanel)
        EVT_PAINT(                 ProfilerPanel::onPaint)
        EVT_MOTION(                ProfilerPanel::onMouseMove)
        EVT_MOUSEWHEEL(            ProfilerPanel::onMouseWheel)
        EVT_LEFT_DOWN(             ProfilerPanel::onMouseDown)
        EVT_RIGHT_DOWN(            ProfilerPanel::onMouseDown)
        EVT_LEFT_UP(               ProfilerPanel::onMouseUp)
        EVT_LEAVE_WINDOW(          ProfilerPanel::onMouseLeave)
        EVT_KEY_UP(                ProfilerPanel::onKeyUp)
        EVT_MENU(ID_SaveView,      ProfilerPanel::onSaveView)
    END_EVENT_TABLE()

    ProfilerPanel::ProfilerPanel(wxWindow &parent, SessionData &sessionData) :
        wxPanel(&parent),
        m_sessionData(sessionData),
        m_font(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Lucida Sans Unicode")
    {
        SetLabel("Profiler View");
        SetBackgroundStyle(wxBG_STYLE_PAINT);

        m_pinToEnd        = true;

        m_lastMousePos    = wxDefaultPosition;
        m_mouseDownPos    = wxDefaultPosition;
        m_mouseButtonDown = false;
        m_dragStartTime   = 0.0;

        m_startTime       = 0.0;
        m_pixelsPerSecond = 1000.0;
    }

    ProfilerPanel::~ProfilerPanel(void)
    {
    }

    double ProfilerPanel::getSecondsPerPixel(void) const
    {
        return 1.0 / m_pixelsPerSecond;
    }

    double ProfilerPanel::getPixelsPerSecond(void) const
    {
        return m_pixelsPerSecond;
    }

    double ProfilerPanel::getTimeForPixel(int x) const
    {
        return m_startTime + getSecondsPerPixel() * x;
    }

    void ProfilerPanel::onPaint(wxPaintEvent &event)
    {
        const TimeRange timerange     = m_sessionData.getTimeRange();
        const double    lastVSyncTime = m_sessionData.getLastVSyncTime();
        wxAutoBufferedPaintDC dc(this);
        const int width = dc.GetSize().x;
        if(m_pinToEnd)
        {
            if(timerange.Contains(lastVSyncTime) && (timerange.max - lastVSyncTime) < 1.0)
            {
                // We have received a valid frame packet... so lets use that as our end time as long as its not super old
                const double maxTime = lastVSyncTime - timerange.min;
                const double endTime = getTimeForPixel(width);
                const double offset  = maxTime - endTime;
                m_startTime += offset;
            }
            else if(timerange.max > timerange.min)
            {
                // We don't have any recent frame packets... so we just use the last event time as our maxTime...
                const double maxTime = timerange.max - timerange.min;
                const double endTime = getTimeForPixel(width);
                const double offset  = maxTime - endTime;
                m_startTime += offset;
            }
        }
        dc.SetFont(m_font);
        m_sessionData.draw(dc, m_lastMousePos, m_startTime+timerange.min, getTimeForPixel(width)+timerange.min);
    }

    void ProfilerPanel::onMouseMove(wxMouseEvent &event)
    {
        m_lastMousePos = event.GetPosition();
        if(m_mouseButtonDown)
        {
            m_startTime = m_dragStartTime + (m_mouseDownPos.x - m_lastMousePos.x) * getSecondsPerPixel();
        }
        Refresh(false);
    }

    void ProfilerPanel::onMouseWheel(wxMouseEvent &event)
    {
        const wxPoint mousePos     = event.GetPosition();
        const double  selectedTime = getTimeForPixel(mousePos.x);
        const double  zoomSpeed    = (m_pixelsPerSecond * 0.1);

        m_pixelsPerSecond += zoomSpeed * ((double)event.GetWheelRotation()) / ((double)event.GetWheelDelta());

        // Clamp the allowable pixelsPerSecond between 400 and 2000000...
        m_pixelsPerSecond = std::max(400.0,     m_pixelsPerSecond);
        m_pixelsPerSecond = std::min(2000000.0, m_pixelsPerSecond);

        m_startTime = selectedTime - (mousePos.x * getSecondsPerPixel());

        Refresh(false);
    }

    void ProfilerPanel::onMouseDown(wxMouseEvent &event)
    {
        if(event.GetButton() == wxMOUSE_BTN_LEFT)
        {
            m_pinToEnd        = false; // disable pinning anytime the user drags...
            m_mouseDownPos    = event.GetPosition();
            m_mouseButtonDown = true;
            m_dragStartTime   = m_startTime;
        }
        else if(event.GetButton() == wxMOUSE_BTN_RIGHT)
        {
            wxMenu menu;
            menu.Append(ID_SaveView, "Save View");
            PopupMenu(&menu);
        }
    }

    void ProfilerPanel::onMouseUp(wxMouseEvent &event)
    {
        m_mouseDownPos    = wxDefaultPosition;
        m_mouseButtonDown = false;
    }

    void ProfilerPanel::onMouseLeave(wxMouseEvent &event)
    {
        m_lastMousePos = wxDefaultPosition;
        Refresh(false);
    }

    void ProfilerPanel::onKeyUp(wxKeyEvent &event)
    {
        const int keycode = event.GetKeyCode();
        if(keycode == WXK_SPACE)
        {
            m_pinToEnd = !m_pinToEnd;
        }
    }

    void ProfilerPanel::onSaveView(wxCommandEvent &event)
    {
        const wxSize    clientSize = GetClientSize();
        const TimeRange timerange  = m_sessionData.getTimeRange();
    #if 0
        // Experimental SVG captures... although seems to be kind of screwy right now...
        wxFileDialog saveFileDialog(this, "Save Chart View", wxEmptyString, wxEmptyString, "SVG File (*.svg)|*.svg", wxFD_SAVE);
        if(saveFileDialog.ShowModal() != wxID_CANCEL)
        {
            wxSVGFileDC dc(saveFileDialog.GetPath(), clientSize.x, clientSize.y, 72);
            dc.SetFont(m_font);
            m_sessionData.draw(dc, m_lastMousePos, m_startTime+timerange.min, getTimeForPixel(clientSize.x)+timerange.min);
        }
    #else
        wxFileDialog saveFileDialog(this, "Save Chart View", wxEmptyString, wxEmptyString, "JPEG File (*.jpg)|*.jpg", wxFD_SAVE);
        if(saveFileDialog.ShowModal() != wxID_CANCEL)
        {
            wxBitmap bmp(clientSize.x, clientSize.y);
            wxMemoryDC dc(bmp);
            dc.SetFont(m_font);
            m_sessionData.draw(dc, m_lastMousePos, m_startTime+timerange.min, getTimeForPixel(clientSize.x)+timerange.min);
            wxImage img = bmp.ConvertToImage();
            img.SetOption("quality", "80");
            img.SaveFile(saveFileDialog.GetPath(), wxBITMAP_TYPE_JPEG);
        }
    #endif
    }


} // namespace Monitor
} // namespace OVR
