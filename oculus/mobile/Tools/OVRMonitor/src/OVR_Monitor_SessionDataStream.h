#ifndef OVR_MONITOR_SESSIONDATASTREAM_H
#define OVR_MONITOR_SESSIONDATASTREAM_H

#include <wx/wx.h>
#include <wx/mstream.h> // for wxMemoryInputStream and wxMemoryOutputStream

#include <vector>
#include <map>
#include <string>

#include <OVR_Capture_Types.h>
#include "OVR_Monitor_Range.h"

namespace OVR
{
namespace Monitor
{

    typedef std::pair<Capture::UInt32, wxString>                    LabelPair;
    typedef std::map<LabelPair::first_type, LabelPair::second_type> LabelMap;

    class SessionDataStream
    {
        public:
            enum Type
            {
                Type_FrameBuffers = 0,
                Type_VSync,
                Type_CPUZones,
                Type_Sensor,
            };

        public:
            SessionDataStream(Type type);
            virtual ~SessionDataStream(void);

            Type         getType(void) const;
            unsigned int getDrawHeight(void) const;
            TimeRange    getTimeRange(void) const;

        public:
            virtual void draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const = 0;
            virtual void drawMouseOver(wxDC &dc, wxPoint mousePos, wxRect chartRect, double timeBegin, double timeEnd) const {}

        protected:
            void drawLabel(wxDC &dc, wxRect chartRect, const wxString &labelText) const;
            void drawValue(wxDC &dc, wxPoint mousePos, const wxString &valueText) const;

        protected:
            const Type   m_type;
            unsigned int m_drawHeight;
            TimeRange    m_timeRange;
    };
    typedef std::vector<SessionDataStream*> SessionDataStreamVector;

    class SensorDataStream : public SessionDataStream
    {
        public:
            SensorDataStream(Capture::UInt32 labelID, const LabelMap &labels);
            virtual ~SensorDataStream(void);

            void setSensorRangeData(float minValue, float maxValue, Capture::SensorInterpolator interpolator, Capture::SensorUnits units);
            void pushSensorData(double timeInSeconds, float value);

        public:
            virtual void draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const;
            virtual void drawMouseOver(wxDC &dc, wxPoint mousePos, wxRect chartRect, double timeBegin, double timeEnd) const;

        private:    
            // Draw in block mode... which is probably what you want for clock rates...
            void drawBlocky(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const;

            // Draw with linear interpolation... which is probably what you want for thermal sensors and other values that tend to ramp up/down smoothly
            void drawLinear(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const;

        private:
            struct DataPair
            {
                double timeInSeconds;
                float  value;
                friend inline bool operator<(double timeInSeconds, const DataPair &d)
                {
                    return timeInSeconds < d.timeInSeconds;
                }
            };
            typedef std::vector<DataPair> DataVector;
            typedef Range<float>          ValueRange;

            // get an iterator to the first visible sample...
            DataVector::const_iterator getFirstSample(double timeBegin) const;

        private:
            const Capture::UInt32       m_labelID;
            const LabelMap             &m_labels;
            Capture::SensorInterpolator m_interpolator;
            Capture::SensorUnits        m_units;
            ValueRange                  m_valueRange;
            DataVector                  m_sensorData;
    };
    typedef std::pair<Capture::UInt32, SensorDataStream*>                                  SensorDataStreamPair;
    typedef std::map<SensorDataStreamPair::first_type, SensorDataStreamPair::second_type>  SensorDataStreamMap;

    class CPUZoneDataStream : public SessionDataStream
    {
        public:
            CPUZoneDataStream(Capture::UInt32 threadID, const LabelMap &labels);
            virtual ~CPUZoneDataStream(void);

            // Called when the connection is terminated... This typically caps off any opening push/pop enter/leave packets.
            void onConnectionClose(double lastKnownTimeInSeconds);

            void     setName(const char *name);
            wxString getName(void) const;

            void pushEnter(Capture::UInt32 labelID, double timeInSeconds);
            void pushLeave(double timeInSeconds);

            void pushLog(Capture::LogPriority priority, double timeInSeconds, wxString msg);

        public:
            virtual void draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const;
            virtual void drawMouseOver(wxDC &dc, wxPoint mousePos, wxRect chartRect, double timeBegin, double timeEnd) const;

        private:
            struct Zone
            {
                Capture::UInt32 labelID;
                double          timeInSeconds;
            };
            typedef std::vector<Zone> ZoneVector;

            // Jump table used to quickly find the first visible zone... searched using std::upper_bounds which
            // assumes sorted data so likely using a binary search.
            struct ZoneJump
            {
                int    zoneIndex;
                double timeInSeconds;
                friend inline bool operator<(double timeInSeconds, const ZoneJump &zj)
                {
                    return timeInSeconds < zj.timeInSeconds;
                }
            };
            typedef std::vector<ZoneJump> ZoneJumpVector;

            struct LogEntry
            {
                Capture::LogPriority priority;
                double               timeInSeconds;
                wxString             message;
                int                  depth;
                friend inline bool operator<(double timeInSeconds, const LogEntry &e)
                {
                    return timeInSeconds < e.timeInSeconds;
                }
            };
            typedef std::vector<LogEntry> LogVector;

        private:
            static const Capture::UInt32 s_invalidLabelID = ~(Capture::UInt32)0;
            static const int             s_blockHeight    = 16;
            static const int             s_blockPadding   = 4;
            static const int             s_iconRadius     = 4;
            const Capture::UInt32        m_threadID;
            const LabelMap              &m_labels;
            wxString                     m_name;
            ZoneVector                   m_zones;
            ZoneJumpVector               m_jumpTable;
            LogVector                    m_log;
            int                          m_currentDepth;
            int                          m_maxDepth;
    };
    typedef std::pair<Capture::UInt32, CPUZoneDataStream*>                                  CPUZoneDataStreamPair;
    typedef std::map<CPUZoneDataStreamPair::first_type, CPUZoneDataStreamPair::second_type> CPUZoneDataStreamMap;

    class FrameBufferDataStream : public SessionDataStream
    {
        public:
            FrameBufferDataStream(void);
            virtual ~FrameBufferDataStream(void);

            void pushFrameBuffer(double timeInSeconds, const wxImage &framebuffer);

        public:
            virtual void draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const;

        private:
            struct FrameBuffer
            {
                double timeInSeconds;
                void  *jpgData;
                size_t jpgDataSize;
                friend inline bool operator<(double timeInSeconds, const FrameBuffer &fb)
                {
                    return timeInSeconds < fb.timeInSeconds;
                }
            };
            typedef std::vector<FrameBuffer> FrameBufferVector;

            typedef std::pair<double, wxBitmap>                               BitmapPair;
            typedef std::map<BitmapPair::first_type, BitmapPair::second_type> BitmapMap;

        private:
            static const unsigned int s_maxCacheSize = 1024;

            int                m_compressionQuality;
            FrameBufferVector  m_framebuffers;
            mutable BitmapMap  m_cache;
    };

    class VSyncDataStream : public SessionDataStream
    {
        public:
            VSyncDataStream(void);
            virtual ~VSyncDataStream(void);

            void pushVSync(double timeInSeconds);

        public:
            virtual void draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const;

        private:
            typedef std::vector<double> DataVector;

        private:
            DataVector m_vsyncEvents;
    };

} // namespace Monitor
} // namespace OVR

#endif

