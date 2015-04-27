#ifndef OVR_MONITOR_SESSIONDATA_H
#define OVR_MONITOR_SESSIONDATA_H

#include <wx/wx.h>
#include <wx/mstream.h> // for wxMemoryInputStream and wxMemoryOutputStream

#include <OVR_Capture_Types.h>

#include "OVR_Monitor_SessionDataStream.h"

namespace OVR
{
namespace Monitor
{

    class Frame;

    class SessionData
    {
        public:
            SessionData(Frame &window);
            ~SessionData(void);

            void lock(void);
            void unlock(void);

            void reset(void);

            void draw(wxDC &dc, wxPoint mousePos, double timeBegin, double timeEnd);

            // Called when the connection is terminated... This typically caps off any opening push/pop enter/leave packets.
            void onConnectionClose(void);

            void setThreadName(Capture::UInt32 threadID, const char *name);
            void setLabel(Capture::UInt32 labelID, const char *name);

            void pushFrame(double timeInSeconds);
            void pushVSync(double timeInSeconds);
            void setSensorRangeData(Capture::UInt32 labelID,  float minValue, float maxValue, Capture::SensorInterpolator interpolator, Capture::SensorUnits units);
            void pushSensorData(Capture::UInt32 labelID,  double timeInSeconds, float value);
            void pushCPUZoneEnter(Capture::UInt32 threadID, Capture::UInt32 labelID, double timeInSeconds);
            void pushCPUZoneLeave(Capture::UInt32 threadID, double timeInSeconds);
            void pushFrameBuffer(double timeInSeconds, const wxImage &framebuffer);
            void pushLog(Capture::UInt32 threadID, Capture::LogPriority priority, double timeInSeconds, wxString msg);

            wxString    getLabelName(Capture::UInt32 labelID) const;
            wxImage     getLastFrameBuffer(void) const;
            TimeRange   getTimeRange(void) const;
            double      getLastVSyncTime(void) const;

        private:
            SensorDataStream  &getSensorStream(Capture::UInt32 labelID);
            CPUZoneDataStream &getCPUZoneStream(Capture::UInt32 threadID);

        private:
            Frame                    &m_window;

            wxCriticalSection         m_lock;

            SessionDataStreamVector   m_streams;
            SensorDataStreamMap       m_sensorStreams;
            CPUZoneDataStreamMap      m_cpuZoneStreams;
            FrameBufferDataStream    *m_framebufferStream;
            VSyncDataStream          *m_vsyncStream;

            TimeRange                 m_timeRange;
            double                    m_lastVSyncTime;

            LabelMap                  m_labels;

            wxImage                   m_lastFrameBuffer;
    };

} // namespace Monitor
} // namespace OVR

#endif
