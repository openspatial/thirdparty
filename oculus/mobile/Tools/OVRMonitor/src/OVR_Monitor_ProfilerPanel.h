#ifndef OVRPROFILERPANEL_H
#define OVRPROFILERPANEL_H

#include <wx/wx.h>

namespace OVR
{
namespace Monitor
{

    class SessionData;

    class ProfilerPanel : public wxPanel
    {
        DECLARE_EVENT_TABLE()
        public:
            ProfilerPanel(::wxWindow &parent, SessionData &sessionData);
            virtual ~ProfilerPanel(void);

        private:
            double getSecondsPerPixel(void) const;
            double getPixelsPerSecond(void) const;
            double getTimeForPixel(int x) const;

            void onPaint(wxPaintEvent &event);

            void onMouseMove(wxMouseEvent &event);
            void onMouseWheel(wxMouseEvent &event);
            void onMouseDown(wxMouseEvent &event);
            void onMouseUp(wxMouseEvent &event);
            void onMouseLeave(wxMouseEvent &event);
            void onKeyUp(wxKeyEvent &event);

            void onSaveView(wxCommandEvent &event);

        private:
            enum
            {
                ID_RefreshTimer = wxID_HIGHEST + 100, // +100 to avoid collisions with Frame
                ID_SaveView,
            };

        private:
            SessionData  &m_sessionData;

            wxFont        m_font;

            bool          m_pinToEnd;

            wxPoint       m_lastMousePos;
            wxPoint       m_mouseDownPos;
            bool          m_mouseButtonDown;
            double        m_dragStartTime;

            double        m_startTime;
            double        m_pixelsPerSecond;
    };

} // namespace Monitor
} // namespace OVR

#endif
