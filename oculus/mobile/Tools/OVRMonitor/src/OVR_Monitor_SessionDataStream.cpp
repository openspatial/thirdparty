#include "OVR_Monitor_SessionDataStream.h"
#include "OVR_Monitor_Settings.h"


namespace OVR
{
namespace Monitor
{

    /********************
    * SessionDataStream *
    ********************/

    SessionDataStream::SessionDataStream(Type type) :
        m_type(type)
    {
        m_drawHeight = 0;
    }

    SessionDataStream::~SessionDataStream(void)
    {
    }

    SessionDataStream::Type SessionDataStream::getType(void) const
    {
        return m_type;
    }

    unsigned int SessionDataStream::getDrawHeight(void) const
    {
        return m_drawHeight;
    }

    TimeRange SessionDataStream::getTimeRange(void) const
    {
        return m_timeRange;
    }

    void SessionDataStream::drawLabel(wxDC &dc, wxRect chartRect, const wxString &labelText) const
    {
        if(!labelText.IsEmpty())
        {
            const wxSize   extents = dc.GetMultiLineTextExtent(labelText);
            const wxPoint  pos     = wxPoint(chartRect.x + 4, chartRect.y + 2);

            // The transparency on this brush might not be supported on all implementations of wxDC, but it works
            // at least on OSX... and we can always force it on everywhere by using wxGCDC if perf allows.
            dc.SetBrush(wxColour(0,0,0,150));
            dc.SetPen(*wxTRANSPARENT_PEN);
            wxRect labelRect;
            labelRect.x = pos.x - 2;
            labelRect.y = pos.y;
            labelRect.width  = extents.x + 4;
            labelRect.height = extents.y;
            dc.DrawRoundedRectangle(labelRect, 4.0);

            // Finally draw text...
            dc.SetTextForeground(wxColour(255,255,255));
            dc.DrawText(labelText, pos);
        }
    }

    void SessionDataStream::drawValue(wxDC &dc, wxPoint mousePos, const wxString &valueText) const
    {
        if(!valueText.IsEmpty())
        {
            const wxSize   extents = dc.GetMultiLineTextExtent(valueText);
            const wxPoint  pos     = mousePos + wxPoint(16,-4);

            // The transparency on this brush might not be supported on all implementations of wxDC, but it works
            // at least on OSX... and we can always force it on everywhere by using wxGCDC if perf allows.
            dc.SetBrush(wxColour(0,0,0,150));
            dc.SetPen(*wxTRANSPARENT_PEN);
            wxRect labelRect;
            labelRect.x = pos.x - 2;
            labelRect.y = pos.y;
            labelRect.width  = extents.x + 4;
            labelRect.height = extents.y;
            dc.DrawRoundedRectangle(labelRect, 4.0);

            // Finally draw text...
            dc.SetTextForeground(wxColour(255,255,255));
            dc.DrawText(valueText, pos);
        }
    }

    /*******************
    * SensorDataStream *
    *******************/

    SensorDataStream::SensorDataStream(Capture::UInt32 labelID, const LabelMap &labels) :
        SessionDataStream(Type_Sensor),
        m_labelID(labelID),
        m_labels(labels)
    {
        m_interpolator   = Capture::Sensor_Interp_Linear;
        m_units          = Capture::Sensor_Unit_None;
        // Force sensor values to always contain the origin... thus min needs to be >=0
        m_valueRange.min = m_valueRange.max = 0;
    }

    SensorDataStream::~SensorDataStream(void)
    {
    }

    void SensorDataStream::setSensorRangeData(float minValue, float maxValue, Capture::SensorInterpolator interpolator, Capture::SensorUnits units)
    {
        m_interpolator   = interpolator;
        m_units          = units;
        m_valueRange.min = std::min(m_valueRange.min, minValue);   
        m_valueRange.max = std::max(m_valueRange.max, maxValue);
    }

    void SensorDataStream::pushSensorData(double timeInSeconds, float value)
    {
        // Once we get data, set our size so we are visible...
        m_drawHeight = 32;

        // wxASSERT(m_sensorData.empty() || timeInSeconds >= m_timeRange.max); // assert disabled because of the isBlocky() hack below
        m_timeRange.min  = std::min(m_timeRange.min,  timeInSeconds);
        m_timeRange.max  = std::max(m_timeRange.max,  timeInSeconds);
        m_valueRange.min = std::min(m_valueRange.min, value);   
        m_valueRange.max = std::max(m_valueRange.max, value);

        // Total hack! Blocky sensor data only updates when the value changes...
        // because of that, the time range needs to project out into infinity as long as
        // the last value was non-zero.
        if(m_interpolator==Capture::Sensor_Interp_Nearest && value!=0)
        {
            m_timeRange.max = std::numeric_limits<double>::max();
        }
        else
        {
            m_timeRange.max = timeInSeconds;
        }

        DataPair p;
        p.timeInSeconds = timeInSeconds;
        p.value         = value;
        m_sensorData.push_back(p);
    }

    void SensorDataStream::draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        dc.GradientFillLinear(chartRect, wxColour(50,50,100), wxColour(100, 100, 200), wxNORTH);

        if(m_sensorData.empty())
            return;

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxColour(115,130,255));

        // Remove 1 pixel at the top so we get a bit of border...
        chartRect.y      += 1;
        chartRect.height -= 1;

        if(m_interpolator==Capture::Sensor_Interp_Nearest)
            drawBlocky(dc, chartRect, timeBegin, timeEnd);
        else
            drawLinear(dc, chartRect, timeBegin, timeEnd);

        // draw chart label...
        LabelMap::const_iterator foundLabel = m_labels.find(m_labelID);
        if(foundLabel != m_labels.end())
        {
            drawLabel(dc, chartRect, wxString::Format( "Sensor: %s", (*foundLabel).second.mb_str().data() ));
        }
    }

    void SensorDataStream::drawMouseOver(wxDC &dc, wxPoint mousePos, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        if(m_sensorData.empty())
            return;

        const ValueRange valueRange     = m_valueRange;
        const float      invValueRange  = 1.0f / (valueRange.max - valueRange.min);

        const double fx = (double)mousePos.x;
        const double fw = (double)chartRect.width;
        const double t  = (fx / fw) * (timeEnd-timeBegin) + timeBegin;

        DataVector::const_iterator begin = getFirstSample(t);
        DataVector::const_iterator next  = m_interpolator==Capture::Sensor_Interp_Nearest ? begin : begin+1;
        if(next == m_sensorData.end())
            return;

        const double t0 = (*begin).timeInSeconds;
        const double t1 = (*next).timeInSeconds;

        const double v0 = (*begin).value;
        const double v1 = (*next).value;

        const double s  = t1>t0 ? (t-t0) / (t1-t0) : 0; // reverse lerp(t0, t1, s)
        const double v  = s*(v1-v0)+v0;                 // lerp(v0, v1, s)

        const float  fy    = (v - valueRange.min) * invValueRange;
        const int    y     = chartRect.y + chartRect.height - (int)(fy * chartRect.height);
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxColour(0,0,0));
        dc.DrawCircle(mousePos.x, y, 2);

        // value scaled based on display units...
        double unitvalue = v;
        switch(m_units)
        {
            // Frequencies...
            case Capture::Sensor_Unit_GHz:
                unitvalue *= 1000.0; // convert from GHz to MHz...
            case Capture::Sensor_Unit_MHz:
                unitvalue *= 1000.0; // convert from MHz to KHz...
            case Capture::Sensor_Unit_KHz:
                unitvalue *= 1000.0; // convert from KHz to Hz...
            case Capture::Sensor_Unit_Hz:
            {
                const char        *postfixes[] = { "Hz", "KHz", "MHz", "GHz" };
                const unsigned int numPostfixes = sizeof(postfixes) / sizeof(postfixes[0]);
                const char        *postfix      = postfixes[0];
                for(unsigned int i=1; i<numPostfixes && unitvalue>=1000.0; i++)
                {
                    unitvalue *= 1.0/1000.0;
                    postfix    = postfixes[i];
                }
                drawValue(dc, mousePos, wxString::Format("%.2f %s", unitvalue, postfix));
                break;
            }

            // Memory Size...
            case Capture::Sensor_Unit_GByte:
                unitvalue *= 1024.0; // convert from GB to MB...
            case Capture::Sensor_Unit_MByte:
                unitvalue *= 1024.0; // convert from MB to KB...
            case Capture::Sensor_Unit_KByte:
                unitvalue *= 1024.0; // convert from KB to B...
            case Capture::Sensor_Unit_Byte:
            {
                const char        *postfixes[] = { "B", "KB", "MB", "GB" };
                const unsigned int numPostfixes = sizeof(postfixes) / sizeof(postfixes[0]);
                const char        *postfix = postfixes[0];
                for(unsigned int i=1; i<numPostfixes && unitvalue>=1024.0; i++)
                {
                    unitvalue *= 1.0/1024.0;
                    postfix    = postfixes[i];
                }
                drawValue(dc, mousePos, wxString::Format("%.2f %s", unitvalue, postfix));
                break;
            }

            // Memory Bandwidth...
            case Capture::Sensor_Unit_GByte_Second:
                unitvalue *= 1024.0; // convert from GB to MB...
            case Capture::Sensor_Unit_MByte_Second:
                unitvalue *= 1024.0; // convert from MB to KB...
            case Capture::Sensor_Unit_KByte_Second:
                unitvalue *= 1024.0; // convert from KB to B...
            case Capture::Sensor_Unit_Byte_Second:
            {
                const char        *postfixes[] = { "B/s", "KB/s", "MB/s", "GB/s" };
                const unsigned int numPostfixes = sizeof(postfixes) / sizeof(postfixes[0]);
                const char        *postfix = postfixes[0];
                for(unsigned int i=1; i<numPostfixes && unitvalue>=1024.0; i++)
                {
                    unitvalue *= 1.0/1024.0;
                    postfix    = postfixes[i];
                }
                drawValue(dc, mousePos, wxString::Format("%.2f %s", unitvalue, postfix));
                break;
            }

            // Default behavior... just print the raw value...
            default:
                drawValue(dc, mousePos, wxString::Format("%.2f", unitvalue));
        }
    }

    // Draw in block mode... which is probably what you want for clock rates...
    void SensorDataStream::drawBlocky(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        const double timeToPixels = ((double)chartRect.width) / (timeEnd-timeBegin);

        const ValueRange valueRange     = m_valueRange;
        const float      invValueRange  = 1.0f / (valueRange.max - valueRange.min);

        // We clamp X values because Windows GDI rendering uses 16-bits of precision and this makes it
        // very easy for us to overflow that when CPU clocks don't change often...
        const double fxmin = 0.0;
        const double fxmax = (timeEnd-timeBegin) * timeToPixels;

        // Find first visible sample...
        DataVector::const_iterator begin = getFirstSample(timeBegin);
        DataVector::const_iterator end   = m_sensorData.end();

        for(DataVector::const_iterator curr=begin; curr!=end; curr++)
        {
            DataVector::const_iterator next = curr+1;

            const DataPair a = *curr;
            
            const double t0 = a.timeInSeconds;
            const double t1 = next!=end ? (*next).timeInSeconds : timeEnd;

            if(t1 < timeBegin)
            {
                // This block lays before the viewable graph area, skip straight to the next one...
                continue;
            }

            if(t0 > timeEnd)
            {
                // This block lays after the viewable graph area, so we are finished rendering this graph...
                break;
            }

            const float  fy0 = (a.value-valueRange.min) * invValueRange;

            if(fy0 == 0)
            {
                // Don't even attempt to render if nothing will be visible...
                continue;
            }

            const double fx0 = std::max(fxmin, (t0-timeBegin) * timeToPixels);
            const double fx1 = std::min(fxmax, (t1-timeBegin) * timeToPixels);

            const int x0 = chartRect.x + (int)fx0;
            const int x1 = chartRect.x + (int)fx1;

            const int y0 = chartRect.height - (int)(fy0 * chartRect.height);

            // Draw as solid...
            wxRect blockRect;
            blockRect.x      = x0;
            blockRect.y      = chartRect.y + y0;
            blockRect.width  = x1 - x0;
            blockRect.height = chartRect.height - y0;
            dc.DrawRectangle(blockRect);
        }
    }

    // Draw with linear interpolation... which is probably what you want for thermal sensors and other values that tend to ramp up/down smoothly
    void SensorDataStream::drawLinear(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        const double timeToPixels = ((double)chartRect.width) / (timeEnd-timeBegin);

        const ValueRange valueRange     = m_valueRange;
        const double     invValueRange  = 1.0 / (valueRange.max - valueRange.min);

        // Find first visible sample...
        DataVector::const_iterator begin = getFirstSample(timeBegin);
        DataVector::const_iterator end   = m_sensorData.end();

        for(DataVector::const_iterator prev=begin,curr=begin+1; curr<end; prev++,curr++)
        {
            const DataPair a = *prev;
            const DataPair b = *curr;
            const double t0 = a.timeInSeconds;
            const double t1 = b.timeInSeconds;

            if(t1 < timeBegin)
            {
                // This block lays before the viewable graph area, skip straight to the next one...
                continue;
            }

            if(t0 > timeEnd)
            {
                // This block lays after the viewable graph area, so we are finished rendering this graph...
                break;
            }

            const double fy0   = (a.value-valueRange.min) * invValueRange;
            const double fy1   = (b.value-valueRange.min) * invValueRange;

            if(fy0==0 && fy1==0)
            {
                // Don't even attempt to render if nothing will be visible...
                continue;
            }

            const double fx0 = (t0-timeBegin) * timeToPixels;
            const double fx1 = (t1-timeBegin) * timeToPixels;

            const int x0 = chartRect.x + (int)fx0;
            const int x1 = chartRect.x + (int)fx1;
            
            if(fy0 > fy1)
            {
                const int y0 = chartRect.y + chartRect.height - (int)(fy0 * chartRect.height);
                const int y1 = chartRect.y + chartRect.height - (int)(fy1 * chartRect.height);
                const wxPoint triangle[3] =
                {
                    wxPoint(x0, y0),
                    wxPoint(x0, y1),
                    wxPoint(x1, y1),
                };
                dc.DrawPolygon(3, triangle, 0, 0, wxWINDING_RULE);
            }
            else if(fy0 < fy1)
            {
                const int y0 = chartRect.y + chartRect.height - (int)(fy0 * chartRect.height);
                const int y1 = chartRect.y + chartRect.height - (int)(fy1 * chartRect.height);
                const wxPoint triangle[3] =
                {
                    wxPoint(x0, y0),
                    wxPoint(x1, y0),
                    wxPoint(x1, y1),
                };
                dc.DrawPolygon(3, triangle, 0, 0, wxWINDING_RULE);
            }
            
            // Draw as solid region...
            const double fymin = std::min(fy0, fy1);
            if(fymin > 0)
            {
                const int ymin = chartRect.height - (int)(fymin * chartRect.height);
                wxRect blockRect;
                blockRect.x      = x0;
                blockRect.y      = chartRect.y + ymin;
                blockRect.width  = x1 - x0;
                blockRect.height = chartRect.height - ymin;
                dc.DrawRectangle(blockRect);
            }
        }
    }

    // get an iterator to the first visible sample...
    SensorDataStream::DataVector::const_iterator SensorDataStream::getFirstSample(double timeBegin) const
    {
        // Find first visible sample...
        DataVector::const_iterator begin = std::upper_bound(m_sensorData.begin(), m_sensorData.end(), timeBegin);
        if(begin != m_sensorData.begin())
        {
            // As long as we are not the first sample, we should go back one...
            // because we are really just looking for the sample just before timeBegin
            begin--;
        }
        return begin;
    }

    /********************
    * CPUZoneDataStream *
    ********************/

    CPUZoneDataStream::CPUZoneDataStream(Capture::UInt32 threadID, const LabelMap &labels) :
        SessionDataStream(Type_CPUZones),
        m_threadID(threadID),
        m_labels(labels)
    {
        m_name         = wxString::Format("%u", m_threadID);
        m_currentDepth = 0;
        m_maxDepth     = 0;
    }

    CPUZoneDataStream::~CPUZoneDataStream(void)
    {
    }

    // Called when the connection is terminated... This typically caps off any opening push/pop enter/leave packets.
    void CPUZoneDataStream::onConnectionClose(double lastKnownTimeInSeconds)
    {
        while(m_currentDepth > 0)
        {
            pushLeave(lastKnownTimeInSeconds);
        }
    }

    void CPUZoneDataStream::setName(const char *name)
    {
        m_name = name;
    }

    wxString CPUZoneDataStream::getName(void) const
    {
        return m_name;
    }

    void CPUZoneDataStream::pushEnter(Capture::UInt32 labelID, double timeInSeconds)
    {
        if(m_currentDepth == 0)
        {
            // Mark "Enter" zones at the bottom of the stack.
            ZoneJump zj;
            zj.zoneIndex     = m_zones.size();
            zj.timeInSeconds = timeInSeconds;
            m_jumpTable.push_back(zj);
        }
        Zone z;
        z.labelID       = labelID;
        z.timeInSeconds = timeInSeconds;
        m_zones.push_back(z);
        m_currentDepth++;
        
        wxASSERT(timeInSeconds >= m_timeRange.max);
        m_timeRange.min  = std::min(m_timeRange.min,  timeInSeconds);
        m_timeRange.max  = std::max(m_timeRange.max,  timeInSeconds);
    }

    void CPUZoneDataStream::pushLeave(double timeInSeconds)
    {
        //wxASSERT(m_currentDepth > 0);
        if(m_currentDepth <= 0)
        {
            printf("CPUZoneDataStream::pushLeave ERROR! thread=%u, m_currentDepth=%d, timeInSeconds=%f\n", m_threadID, m_currentDepth, timeInSeconds);
            return;
        }

        Zone z;
        z.labelID       = s_invalidLabelID;
        z.timeInSeconds = timeInSeconds;
        m_zones.push_back(z);
        m_maxDepth   = std::max(m_maxDepth, m_currentDepth);
        m_currentDepth--;
        m_drawHeight = m_maxDepth > 0 ? m_maxDepth * (s_blockHeight+s_blockPadding) + s_blockPadding + s_iconRadius*2 : 0;

        wxASSERT(timeInSeconds >= m_timeRange.max);
        m_timeRange.min  = std::min(m_timeRange.min,  timeInSeconds);
        m_timeRange.max  = std::max(m_timeRange.max,  timeInSeconds);
    }

    void CPUZoneDataStream::pushLog(Capture::LogPriority priority, double timeInSeconds, wxString msg)
    {
        LogEntry entry;
        entry.priority      = priority;
        entry.timeInSeconds = timeInSeconds;
        entry.message       = msg;
        entry.depth         = m_currentDepth;
        m_log.push_back(entry);
    }

    void CPUZoneDataStream::draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        dc.GradientFillLinear(chartRect, wxColour(100,20,20), wxColour(180, 50, 50), wxNORTH);

        if(m_jumpTable.empty() || m_zones.empty())
            return;

        dc.SetPen(wxColour(60, 60, 80));
        dc.SetBrush(wxColour(140,140,180));
        dc.SetTextForeground(wxColour(255,255,255));

        const double timeToPixels = ((double)chartRect.width) / (timeEnd-timeBegin);

        const int blockHeight      = s_blockHeight;
        const int blockPadding     = s_blockPadding;
        const int labelYOffset     = (blockHeight - dc.GetCharHeight()) / 2;
        const int blockYIncrement  = blockHeight + blockPadding;
        const int minLabelDrawSize = 32;

        // We clamp X values because Windows GDI rendering uses 16-bits of precision and this makes it
        // very easy for us to overflow that when CPU clocks don't change often...
        // This also has the benefit of keeping zone lables on screen.
        // We add some padding just so we don't show the border of the zone when it extends off screen...
        const double fxmin = -2.0;
        const double fxmax = (timeEnd-timeBegin) * timeToPixels + 2.0;

        // Find first visible zone...
        ZoneJumpVector::const_iterator zjbegin = std::upper_bound(m_jumpTable.begin(), m_jumpTable.end(), timeBegin);
        if(zjbegin != m_jumpTable.begin())
            zjbegin--; // Go back one to get zones that are half off the left of the screen.
        ZoneVector::const_iterator     begin   = m_zones.begin() + (*zjbegin).zoneIndex;
        ZoneVector::const_iterator     end     = m_zones.end();

        ZoneVector stack;
        for(ZoneVector::const_iterator i=begin; i!=end; i++)
        {
            const Zone curr = *i;
            if(curr.labelID != s_invalidLabelID)
            {
                stack.push_back(curr);
            }
            else
            {
                wxASSERT(!stack.empty());
                const Zone leave = curr;
                const Zone enter = stack.back();
                stack.pop_back();
                const int depth = stack.size();

                if(leave.timeInSeconds < timeBegin)
                {
                    // This block lays before the viewable graph area, skip straight to the next one...
                    continue;
                }
                if(enter.timeInSeconds > timeEnd && depth==0)
                {
                    // This block lays after the viewable graph area, so we are finished rendering this graph...
                    break;
                }

                const double fx0    = std::max(fxmin, (enter.timeInSeconds - timeBegin) * timeToPixels);
                const double fx1    = std::min(fxmax, (leave.timeInSeconds - timeBegin) * timeToPixels);
                const double fwidth = fx1-fx0;

                // Skip rendering sub-pixel thick zones.
                if(fwidth < 1.0)
                    continue;

                const int x0 = chartRect.x + (int)fx0;
                const int y0 = chartRect.y+blockPadding + depth*blockYIncrement;

                if(fwidth < 2.0)
                {
                    // Pixel width zones we just render as lines...
                    dc.DrawLine(x0, y0, x0, y0+blockHeight);
                }
                else
                {
                    const int width = std::max(1, (int)fwidth);

                    wxRect zoneRect;
                    zoneRect.x      = x0;
                    zoneRect.y      = y0;
                    zoneRect.width  = width;
                    zoneRect.height = blockHeight;
                    dc.DrawRectangle(zoneRect);

                    // only try and draw labels when the zone is bigger than 'minLabelDrawSize' pixels wide...
                    LabelMap::const_iterator foundLabel = width>minLabelDrawSize ? m_labels.find(enter.labelID) : m_labels.end();
                    if(foundLabel != m_labels.end())
                    {
                        const wxString name                  = (*foundLabel).second;
                        const double   deltaTime             = leave.timeInSeconds - enter.timeInSeconds;
                        const double   deltaTimeMilliseconds = deltaTime*1000.0;
                        const int      labelXOffset          = 4;
                        const int      labelX                = x0 + labelXOffset;
                        const int      labelY                = y0 + labelYOffset;

                        const wxString labelShort     = name;
                        const wxString labelVeryShort = wxString::Format("%.2fms", deltaTimeMilliseconds);
                        const wxString labelLong      = name + wxString(" ") + labelVeryShort;
                        
                        if(dc.GetTextExtent(labelLong).x+labelXOffset < width)
                            dc.DrawText(labelLong, labelX, labelY);
                        else if(dc.GetTextExtent(labelShort).x+labelXOffset < width)
                            dc.DrawText(labelShort, labelX, labelY);
                        else if(dc.GetTextExtent(labelVeryShort).x+labelXOffset < width)
                            dc.DrawText(labelVeryShort, labelX, labelY);
                    }
                } // else if(width > 1)
            }
        }

        // If we have items left in the stack, it either means we are still reading in data or that we stalled in which case be nice to know where we stalled.
        dc.SetPen(wxColour(60, 60, 80));
        dc.SetBrush(wxColour(255,0,0));
        while(stack.size() > 0)
        {
            const Zone enter = stack.back(); stack.pop_back();
            const int  depth = stack.size();

            if(enter.timeInSeconds > timeEnd)
                continue;

            const double fx0    = (enter.timeInSeconds - timeBegin) * timeToPixels;
            const double fx1    = (timeEnd             - timeBegin) * timeToPixels;
            const double fwidth = fx1-fx0;

            const int x0 = chartRect.x + (int)fx0;
            const int y0 = chartRect.y+blockPadding + depth*blockYIncrement;
            const int width = std::max(1, (int)fwidth);

            wxRect zoneRect;
            zoneRect.x      = x0;
            zoneRect.y      = y0;
            zoneRect.width  = width;
            zoneRect.height = blockHeight;
            dc.DrawRectangle(zoneRect);

            // only try and draw labels when the zone is bigger than 'minLabelDrawSize' pixels wide...
            LabelMap::const_iterator foundLabel = width>minLabelDrawSize ? m_labels.find(enter.labelID) : m_labels.end();
            if(foundLabel != m_labels.end())
            {
                const int      labelXOffset = 4;
                const int      labelX       = x0 + labelXOffset;
                const int      labelY       = y0 + labelYOffset;
                const wxString labelShort   = (*foundLabel).second;
                if(dc.GetTextExtent(labelShort).x+labelXOffset < width)
                    dc.DrawText(labelShort, labelX, labelY);
            }
        }

        // Draw log messages... Color coded and overlayed with CPU zones.
        LogVector::const_iterator logbegin = std::upper_bound(m_log.begin(), m_log.end(), timeBegin);
        LogVector::const_iterator logend   = m_log.end();
        dc.SetPen(*wxTRANSPARENT_PEN);
        for(LogVector::const_iterator i=logbegin; i!=logend; i++)
        {
            const LogEntry &entry = *i;

            // Log entry is before the visible range... just skip to the next one.
            if(entry.timeInSeconds < timeBegin)
                continue;

            // Log entry is beyond the visible range... we are done!
            if(entry.timeInSeconds > timeEnd)
                break;

            const double fx0 = (entry.timeInSeconds - timeBegin) * timeToPixels;
            const double fy0 = chartRect.y+blockPadding + (entry.depth-1)*blockYIncrement + blockHeight + 4;

            const int x0 = (int)fx0;
            const int y0 = (int)fy0;

            switch(entry.priority)
            {
                case Capture::Log_Info:
                {
                    dc.SetBrush(wxColour(0,0,255));
                    dc.DrawCircle(x0, y0, s_iconRadius);
                    break;
                }
                case Capture::Log_Warning:
                {
                    dc.SetBrush(wxColour(255,255,0));
                    dc.DrawRectangle(x0-s_iconRadius, y0-s_iconRadius, s_iconRadius*2, s_iconRadius*2);
                    break;
                }
                case Capture::Log_Error:
                {
                    const int r = s_iconRadius;
                    const wxPoint points[] = 
                    {
                        wxPoint(-r, 0),
                        wxPoint( 0, r),
                        wxPoint( r, 0),
                        wxPoint( 0,-r),
                    };
                    dc.SetBrush(wxColour(255,0,0));
                    dc.DrawPolygon(4, points, x0, y0, wxWINDING_RULE);
                    break;
                }
            }
        }

        // draw chart label...
        // TODO: get the thread "name" via pthread_getname_np
        drawLabel(dc, chartRect, wxString("CPU Thread: ") + m_name);
    }

    void CPUZoneDataStream::drawMouseOver(wxDC &dc, wxPoint mousePos, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        const double timeToPixels = ((double)chartRect.width) / (timeEnd-timeBegin);

        const int blockHeight      = s_blockHeight;
        const int blockPadding     = s_blockPadding;
        const int blockYIncrement  = blockHeight + blockPadding;

        // Draw tooltip for selected log messages...
        wxString tpmessage;
        LogVector::const_iterator logbegin = std::upper_bound(m_log.begin(), m_log.end(), timeBegin);
        LogVector::const_iterator logend   = m_log.end();
        dc.SetPen(*wxTRANSPARENT_PEN);
        for(LogVector::const_iterator i=logbegin; i!=logend; i++)
        {
            const LogEntry &entry = *i;

            // Log entry is before the visible range... just skip to the next one.
            if(entry.timeInSeconds < timeBegin)
                continue;

            // Log entry is beyond the visible range... we are done!
            if(entry.timeInSeconds > timeEnd)
                break;

            const double fx0 = (entry.timeInSeconds - timeBegin) * timeToPixels;
            const double fy0 = chartRect.y+blockPadding + (entry.depth-1)*blockYIncrement + blockHeight + 4;

            const int x0 = (int)fx0;
            const int y0 = (int)fy0;

            const wxRect rect(x0-s_iconRadius, y0-s_iconRadius, s_iconRadius*2, s_iconRadius*2);
            if(rect.Contains(mousePos))
            {
                if(!tpmessage.IsEmpty()) tpmessage += "\n";
                tpmessage += entry.message;
            }
        }
        if(!tpmessage.IsEmpty())
        {
            drawValue(dc, mousePos, tpmessage);
        }
    }

    /************************
    * FrameBufferDataStream *
    ************************/

    FrameBufferDataStream::FrameBufferDataStream(void) :
        SessionDataStream(Type_FrameBuffers)
    {
        m_drawHeight         = 128;
        m_compressionQuality = Settings().GetFrameBufferCompressionQuality(); // This change only takes affect on app restart
    }

    FrameBufferDataStream::~FrameBufferDataStream(void)
    {
        for(FrameBufferVector::iterator i=m_framebuffers.begin(); i!=m_framebuffers.end(); i++)
        {
            free((*i).jpgData);
        }
        m_framebuffers.clear();
    }

    void FrameBufferDataStream::pushFrameBuffer(double timeInSeconds, const wxImage &framebuffer)
    {
        FrameBuffer fb;
        fb.timeInSeconds = timeInSeconds;
        // Testing on MacBookPro 128x128 input image from Cinema jpeg compression takes ~490.00us, results in ~3819 bytes.
        // So probably don't need to run this on a worker thread quite yet.
        wxMemoryOutputStream outputStream;
        wxImage temp = framebuffer;
        temp.SetOption("quality", wxString::Format("%d", m_compressionQuality));
        if(temp.SaveFile(outputStream, wxBITMAP_TYPE_JPEG))
        {
            fb.jpgDataSize = outputStream.GetSize();
            fb.jpgData     = malloc(fb.jpgDataSize);
            outputStream.CopyTo(fb.jpgData, fb.jpgDataSize);
        }
        m_framebuffers.push_back(fb);

        wxASSERT(timeInSeconds >= m_timeRange.max);
        m_timeRange.min  = std::min(m_timeRange.min,  timeInSeconds);
        m_timeRange.max  = std::max(m_timeRange.max,  timeInSeconds);
    }

    void FrameBufferDataStream::draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        const unsigned int drawHeight = getDrawHeight();
        if(!drawHeight)
            return;

        dc.GradientFillLinear(chartRect, wxColour(50,50,50), wxColour(100, 100, 100), wxNORTH);

        if(m_framebuffers.empty())
            return;

        const double pixelsPerSecond = ((double)chartRect.width) / (timeEnd-timeBegin);
        const double frameTime       = 1.0 / 60.0;
        const double frameWidth      = pixelsPerSecond * frameTime;

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxColour(0,0,0));

        // Find first visible framebuffer....
        FrameBufferVector::const_iterator begin = std::upper_bound(m_framebuffers.begin(), m_framebuffers.end(), timeBegin);
        FrameBufferVector::const_iterator end   = m_framebuffers.end();
        if(begin != m_framebuffers.begin())
        {
            // rewind once to grab any framebuffers that lay slightly off the left of the screen
            begin--;
        }

        for(FrameBufferVector::const_iterator i=begin; i!=end; i++)
        {
            const FrameBuffer &fb = *i;
            const double t0  = fb.timeInSeconds;
            const double fx0 = (t0-timeBegin) * pixelsPerSecond;
            const int    x0  = chartRect.x + (int)fx0;

            if(x0 > chartRect.width)
            {
                // This block lays after the viewable graph area, so we are finished rendering this graph...
                break;
            }

            if(frameWidth > 8)
            {
                wxBitmap bitmap;
                BitmapMap::iterator found = m_cache.find(fb.timeInSeconds);
                if(found != m_cache.end())
                {
                    // bitmap already in cache... pull it out...
                    bitmap = (*found).second;
                }
                else
                {
                    // bitmap not in cache... decode and add it...
                    wxImage img;
                    wxMemoryInputStream jpgStream(fb.jpgData, fb.jpgDataSize);
                    img.LoadFile(jpgStream, wxBITMAP_TYPE_JPEG);
                    if(img.GetHeight() != (int)drawHeight)
                    {
                        // Scale the image to fit the timeline...
                        const double drawScale = ((double)drawHeight) / (double)img.GetHeight();
                        const int    drawWidth = (int)(img.GetWidth()*drawScale);
                        img.Rescale(drawWidth, drawHeight, wxIMAGE_QUALITY_HIGH);
                    }
                    bitmap = wxBitmap(img);
                    wxASSERT(bitmap.IsOk());
                    if(bitmap.IsOk())
                    {
                        m_cache.insert(BitmapPair(fb.timeInSeconds, bitmap));
                    }
                }
                wxASSERT(bitmap.IsOk());
                if(bitmap.IsOk())
                {
                    dc.DrawBitmap(bitmap, x0, chartRect.y);
                }
            }
            else
            {
                wxRect rect = chartRect;
                rect.x     = x0;
                rect.width = 1;//x1-x0;
                dc.DrawRectangle(rect);
            }
        }

        // If our bitmap cache has grown too large, lets nuke everything out of view
        if(m_cache.size() > s_maxCacheSize)
        {
            // Find the beginning of the visible region...
            BitmapMap::iterator viewBegin;
            for(viewBegin=m_cache.begin(); viewBegin!=m_cache.end(); viewBegin++)
                if((*viewBegin).first >= timeBegin)
                    break;
            // Now delete elements that land *before* the visible region....
            m_cache.erase(m_cache.begin(), viewBegin);

            // Find the end of the visible region...
            BitmapMap::iterator viewEnd;
            for(viewEnd=m_cache.begin(); viewEnd!=m_cache.end(); viewEnd++)
                if((*viewEnd).first > timeEnd)
                    break;
            // Now delete elements that land *before* the visible region....
            m_cache.erase(viewEnd, m_cache.end());
        }
    }

    /******************
    * VSyncDataStream *
    ******************/

    VSyncDataStream::VSyncDataStream(void) :
        SessionDataStream(Type_VSync)
    {
        // Always draw the timeline...
        m_drawHeight    = 20;
        m_timeRange.min = std::numeric_limits<double>::min();
        m_timeRange.max = std::numeric_limits<double>::max();
    }

    VSyncDataStream::~VSyncDataStream(void)
    {
    }

    void VSyncDataStream::pushVSync(double timeInSeconds)
    {
        m_vsyncEvents.push_back(timeInSeconds);
    }

    void VSyncDataStream::draw(wxDC &dc, wxRect chartRect, double timeBegin, double timeEnd) const
    {
        dc.GradientFillLinear(chartRect, wxColour(100,100,50), wxColour(200, 200, 100), wxNORTH);

        const double pixelsPerSecond = ((double)chartRect.width) / (timeEnd-timeBegin);

        // Draw VSync Events...
        dc.SetPen(wxColour(0,0,0,100));
        DataVector::const_iterator begin = std::upper_bound(m_vsyncEvents.begin(), m_vsyncEvents.end(), timeBegin);
        DataVector::const_iterator end   = m_vsyncEvents.end();
        for(DataVector::const_iterator i=begin; i!=end; i++)
        {
            const double t0  = *i;

            if(t0 < timeBegin)
            {
                // This event lays before the start of the visible range, so just skip to the next one and try again...
                continue;
            }

            if(t0 > timeEnd)
            {
                // This event lays after the end of the visible range, so we are done... just quit.
                break;
            }

            const double fx0 = (t0-timeBegin) * pixelsPerSecond;
            const int    x0  = (int)fx0;
            dc.DrawLine(x0, chartRect.y, x0, chartRect.y+chartRect.height);
        }

        drawLabel(dc, chartRect, "VSync");
    }

} // namespace Monitopr
} // namespace OVR
