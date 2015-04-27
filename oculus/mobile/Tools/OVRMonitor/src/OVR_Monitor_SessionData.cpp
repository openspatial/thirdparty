#include "OVR_Monitor_SessionData.h"
#include "OVR_Monitor_Frame.h"

#include <algorithm>

namespace OVR
{
namespace Monitor
{

    static bool StreamCompare(const SessionDataStream *a, const SessionDataStream *b)
    {
        if(a->getType() < b->getType())
            return true;
        return false;
    }

    /**************
    * SessionData *
    **************/

    SessionData::SessionData(Frame &window) :
        m_window(window)
    {
        m_framebufferStream = NULL;
        m_vsyncStream       = NULL;
        m_lastVSyncTime     = 0;
        reset();
    }

    SessionData::~SessionData(void)
    {
        reset();
    }

    void SessionData::lock(void)
    {
        m_lock.Enter();
    }

    void SessionData::unlock(void)
    {
        m_lock.Leave();
    }

    void SessionData::reset(void)
    {
        m_timeRange     = TimeRange();
        m_lastVSyncTime = 0;

        // delete sensor streams...
        for(SensorDataStreamMap::iterator i=m_sensorStreams.begin(); i!=m_sensorStreams.end(); i++)
            delete (*i).second;
        m_sensorStreams.clear();

        // delete cpu zone streams...
        for(CPUZoneDataStreamMap::iterator i=m_cpuZoneStreams.begin(); i!=m_cpuZoneStreams.end(); i++)
            delete (*i).second;
        m_cpuZoneStreams.clear();

        // delete the single framebuffer stream...
        if(m_framebufferStream) delete m_framebufferStream;
        m_framebufferStream = NULL;

        // Delete the single vsync stream...
        if(m_vsyncStream) delete m_vsyncStream;
        m_vsyncStream = NULL;

        // remove all the streams from being rendered...
        m_streams.clear();
    }

    void SessionData::draw(wxDC &dc, wxPoint mousePos, double timeBegin, double timeEnd)
    {
        const wxSize clientSize = dc.GetSize();
        wxRect clientRect;
        clientRect.x = clientRect.y = 0;
        clientRect.width  = clientSize.x;
        clientRect.height = clientSize.y;

        wxRect                   chartRectMouseOver;
        const SessionDataStream *dataStreamMouseOver = NULL;

        const TimeRange timeRange(timeBegin, timeEnd);

        lock();

        // Sort streams into a predictable order...
        std::sort(m_streams.begin(), m_streams.end(), StreamCompare);

        // Loop over all visible signals and draw...
        for(SessionDataStreamVector::const_iterator i=m_streams.begin(); i!=m_streams.end(); i++)
        {
            const SessionDataStream *dataStream = *i;
            wxRect chartRect = clientRect;
            chartRect.height = dataStream->getDrawHeight();

            // Skip hidden data streams...
            if(chartRect.height <= 0)
                continue;

            // Skip data streams that don't have any data in view...
            // We inflate by a bit just to make sure the charts don't flicker when you zoom into the head of the stream.
            if(dataStream->getType()!=SessionDataStream::Type_FrameBuffers && !timeRange.Inflate(2.0).Intersects(dataStream->getTimeRange()))
                continue;

            dataStream->draw(dc, chartRect, timeBegin, timeEnd);

            // Remember this data stream if the mouse is over it...
            if(!dataStreamMouseOver && chartRect.Contains(mousePos))
            {
                dataStreamMouseOver = dataStream;
                chartRectMouseOver  = chartRect;
            }

            // Shrink client rect based on the current chart size...
            clientRect.y += chartRect.height;
            clientRect.height = clientSize.y - clientRect.y;
            if(clientRect.height <= 0)
                break;
        }

        if(dataStreamMouseOver)
        {
            // Draw mouse over layer... usually includes data hilights, etc... we do this last so that
            // it can overdraw neighboring charts if needed...
            dataStreamMouseOver->drawMouseOver(dc, mousePos, chartRectMouseOver, timeBegin, timeEnd);
        }

        unlock();

        // Fill remaining screen area...
        if(clientRect.height > 0)
        {
            dc.GradientFillLinear(clientRect, wxColour(40,40,50), wxColour(60, 90, 90), wxNORTH);
        }

        // Draw mouse "blade"
        if(mousePos.x>=0 && mousePos.x<clientSize.x)
        {
            dc.SetPen(wxColour(255,255,0,150)); // semi-transparent yellow...
            dc.DrawLine(mousePos.x, 0, mousePos.x, clientRect.y);
        }

        // Draw the selected time...
        if(mousePos.x>=0 && mousePos.x<clientSize.x && clientRect.height > 0 && timeEnd > timeBegin)
        {
            const double secondsPerPixel = (timeEnd-timeBegin) / (double)clientRect.width;
            const double tm              = mousePos.x*secondsPerPixel + (timeBegin - getTimeRange().min);
            const wxString timeText      = wxString::Format("%.4fs", tm);
            const wxSize   extents       = dc.GetTextExtent(timeText);
            const int      carrotSize    = 6;
            const wxPoint  pos           = wxPoint(mousePos.x-extents.x/2, clientRect.y+carrotSize);

            dc.SetBrush(wxColour(0,0,0,150));
            dc.SetPen(wxNullPen);
            dc.SetTextForeground(wxColour(255,255,255));

            // Draw carrot pointing from box to blade...
            const wxPoint carrotPoints[3] =
            {
                wxPoint(mousePos.x,            pos.y-carrotSize),
                wxPoint(mousePos.x-carrotSize, pos.y),
                wxPoint(mousePos.x+carrotSize, pos.y),
            };
            dc.DrawPolygon(3, carrotPoints, 0, 0, wxWINDING_RULE);

            // Draw bounding box...
            wxRect labelRect;
            labelRect.x = pos.x - 2;
            labelRect.y = pos.y;
            labelRect.width  = extents.x + 4;
            labelRect.height = extents.y;
            dc.DrawRoundedRectangle(labelRect, 4.0);

            // Draw text...
            dc.DrawText(timeText, pos);
        }
        
    }

    // Called when the connection is terminated... This typically caps off any opening push/pop enter/leave packets.
    void SessionData::onConnectionClose(void)
    {
        for(CPUZoneDataStreamMap::iterator i=m_cpuZoneStreams.begin(); i!=m_cpuZoneStreams.end(); i++)
        {
            (*i).second->onConnectionClose(m_timeRange.max);
        }
        for(SensorDataStreamMap::iterator i=m_sensorStreams.begin(); i!=m_sensorStreams.end(); i++)
        {
            (*i).second->pushSensorData(m_timeRange.max, 0);
        }
    }

    void SessionData::setThreadName(Capture::UInt32 threadID, const char *name)
    {
        getCPUZoneStream(threadID).setName(name);
    }

    void SessionData::setLabel(Capture::UInt32 labelID, const char *name)
    {
        LabelMap::iterator found = m_labels.find(labelID);
        if(found != m_labels.end())
        {
            wxASSERT((*found).second == name);
            (*found).second = name;
        }
        else
        {
            m_labels.insert(LabelPair(labelID, name));
        }
    }

    void SessionData::pushFrame(double timeInSeconds)
    {
        // Global time range...
        m_timeRange.min = std::min(m_timeRange.min, timeInSeconds);
        m_timeRange.max = std::max(m_timeRange.max, timeInSeconds);
    }

    void SessionData::pushVSync(double timeInSeconds)
    {
        if(!m_vsyncStream)
        {
            m_vsyncStream = new VSyncDataStream();
            m_streams.push_back(m_vsyncStream);
        }

        m_lastVSyncTime = timeInSeconds;
        m_vsyncStream->pushVSync(timeInSeconds);

        // Post a message to the main frame indicating that we have new data available...
        wxCommandEvent event(NewDataEvent); 
        wxPostEvent(&m_window, event);
    }

    void SessionData::setSensorRangeData(Capture::UInt32 labelID, float minValue, float maxValue, Capture::SensorInterpolator interpolator, Capture::SensorUnits units)
    {
        getSensorStream(labelID).setSensorRangeData(minValue, maxValue, interpolator, units);
    }

    void SessionData::pushSensorData(Capture::UInt32 labelID, double timeInSeconds, float value)
    {
        getSensorStream(labelID).pushSensorData(timeInSeconds, value);

        // Global time range...
        m_timeRange.min = std::min(m_timeRange.min, timeInSeconds);
        m_timeRange.max = std::max(m_timeRange.max, timeInSeconds);
    }

    void SessionData::pushCPUZoneEnter(Capture::UInt32 threadID, Capture::UInt32 labelID, double timeInSeconds)
    {
        getCPUZoneStream(threadID).pushEnter(labelID, timeInSeconds);

        // Global time range...
        m_timeRange.min = std::min(m_timeRange.min, timeInSeconds);
        m_timeRange.max = std::max(m_timeRange.max, timeInSeconds);
    }

    void SessionData::pushCPUZoneLeave(Capture::UInt32 threadID, double timeInSeconds)
    {
        getCPUZoneStream(threadID).pushLeave(timeInSeconds);

        // Global time range...
        m_timeRange.min = std::min(m_timeRange.min, timeInSeconds);
        m_timeRange.max = std::max(m_timeRange.max, timeInSeconds);
    }

    void SessionData::pushFrameBuffer(double timeInSeconds, const wxImage &framebuffer)
    {
        if(!m_framebufferStream)
        {
            m_framebufferStream = new FrameBufferDataStream();
            m_streams.push_back(m_framebufferStream);
        }

        // Uncompressed version ready for immediate viewing...
        m_lastFrameBuffer = framebuffer;
        m_framebufferStream->pushFrameBuffer(timeInSeconds, framebuffer);

        // Global time range...
        m_timeRange.min = std::min(m_timeRange.min, timeInSeconds);
        m_timeRange.max = std::max(m_timeRange.max, timeInSeconds);

        // Post a message to the main frame indicating that we have new data available...
        wxCommandEvent event(NewDataEvent); 
        wxPostEvent(&m_window, event);
    }

    void SessionData::pushLog(Capture::UInt32 threadID, Capture::LogPriority priority, double timeInSeconds, wxString msg)
    {
        CPUZoneDataStream &stream = getCPUZoneStream(threadID);
        stream.pushLog(priority, timeInSeconds, msg);

        // Post a message to the main frame indicating that we have new data available...
        NewLogEvent event;
        event.threadID      = threadID;
        event.threadName    = stream.getName();
        event.priority      = priority;
        event.timeInSeconds = timeInSeconds;
        event.message       = msg;
        wxPostEvent(&m_window, event);
    }

    wxString SessionData::getLabelName(Capture::UInt32 labelID) const
    {
        LabelMap::const_iterator found = m_labels.find(labelID);
        if(found != m_labels.end())
            return (*found).second;
        return wxString();
    }

    wxImage SessionData::getLastFrameBuffer(void) const
    {
        return m_lastFrameBuffer;
    }

    TimeRange SessionData::getTimeRange(void) const
    {
        return m_timeRange;
    }

    double SessionData::getLastVSyncTime(void) const
    {
        return m_lastVSyncTime;
    }

    SensorDataStream &SessionData::getSensorStream(Capture::UInt32 labelID)
    {
        SensorDataStream *sensorStream = NULL;
        SensorDataStreamMap::iterator found = m_sensorStreams.find(labelID);
        if(found == m_sensorStreams.end())
        {
            sensorStream = new SensorDataStream(labelID, m_labels);
            m_streams.push_back(sensorStream);
            m_sensorStreams.insert(SensorDataStreamPair(labelID, sensorStream));
        }
        else
        {
            sensorStream = (*found).second;
        }
        wxASSERT(sensorStream);
        return *sensorStream;
    }

    CPUZoneDataStream &SessionData::getCPUZoneStream(Capture::UInt32 threadID)
    {
        CPUZoneDataStream *stream = NULL;
        CPUZoneDataStreamMap::iterator found = m_cpuZoneStreams.find(threadID);
        if(found == m_cpuZoneStreams.end())
        {
            stream = new CPUZoneDataStream(threadID, m_labels);
            m_streams.push_back(stream);
            m_cpuZoneStreams.insert(CPUZoneDataStreamPair(threadID, stream));
        }
        else
        {
            stream = (*found).second;
        }
        wxASSERT(stream);
        return *stream;
    }

} // namespace Monitor
} // namespace OVR
