#include "OVR_Monitor_FrameViewerPanel.h"
#include "OVR_Monitor_SessionData.h"

namespace OVR
{
namespace Monitor
{

    /********************
    * FrameViewerCanvas *
    ********************/

    class FrameViewerCanvas : public wxPanel
    {
        DECLARE_EVENT_TABLE()
        public:
            FrameViewerCanvas(wxWindow &parent, SessionData &sessionData) :
                wxPanel(&parent, wxID_ANY, wxDefaultPosition, wxSize(512, 512), wxBORDER_RAISED),
                m_sessionData(sessionData)
            {
                SetBackgroundStyle(wxBG_STYLE_PAINT);
            }

            ~FrameViewerCanvas(void)
            {
            }

            void onPaint(wxPaintEvent &event)
            {
                //wxStopWatch stopwatch;

                wxPaintDC dc(this);
                const wxSize clientSize = GetClientSize();

                m_sessionData.lock();
                wxImage framebuffer = m_sessionData.getLastFrameBuffer();
                m_sessionData.unlock();

                if(framebuffer.IsOk())
                {
                    if(clientSize != framebuffer.GetSize())
                    {
                        framebuffer = framebuffer.Scale(clientSize.x, clientSize.y);//, wxIMAGE_QUALITY_HIGH);
                    }
                    dc.DrawBitmap(wxBitmap(framebuffer), 0, 0);
                }
                else
                {
                    dc.SetBackground(wxColour(0,0,0));
                    dc.Clear();
                    const wxFont font(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, wxT("Lucida Sans Unicode"));
                    dc.SetFont(font);
                    dc.SetTextForeground(wxColour(255,255,255));
                    const wxString noSignalString = "No Signal";
                    const wxSize   noSignalSize   = dc.GetTextExtent(noSignalString);
                    dc.DrawText(noSignalString, (clientSize.x-noSignalSize.x)/2, (clientSize.y-noSignalSize.y)/2);
                }
                
                //printf("FrameViewerCanvas::onPaint Took %.2fms\n", stopwatch.TimeInMicro().ToDouble()/1000.0);
            }

        private:
            enum
            {
                ID_RefreshTimer = wxID_HIGHEST + 100, // +100 to avoid collisions with Frame
            };

        private:
            SessionData &m_sessionData;
    };

    BEGIN_EVENT_TABLE(FrameViewerCanvas, wxPanel)
        EVT_PAINT(    FrameViewerCanvas::onPaint)
    END_EVENT_TABLE()

    /*******************
    * FrameViewerPanel *
    ********************/

    BEGIN_EVENT_TABLE(FrameViewerPanel, wxPanel)
    END_EVENT_TABLE()

    FrameViewerPanel::FrameViewerPanel(wxWindow &parent, SessionData &sessionData) :
        wxPanel(&parent)
    {
        SetLabel("Framebuffer Viewer");


        FrameViewerCanvas *canvas = new FrameViewerCanvas(*this, sessionData);

        wxBoxSizer *sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(canvas, 0, wxALL | wxALIGN_CENTRE, 20);
        SetSizer(sizer);
    }

    FrameViewerPanel::~FrameViewerPanel(void)
    {
    }

} // namespace Monitor
} // namespace OVR
