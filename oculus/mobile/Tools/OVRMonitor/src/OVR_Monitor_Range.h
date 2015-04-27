#ifndef OVR_MONITOR_RANGE_H
#define OVR_MONITOR_RANGE_H

#include <limits>
#include <algorithm>

namespace OVR
{
namespace Monitor
{

    template<typename T>
    struct Range
    {
        Range(void)
        {
            // Initialized to an invalid state...
            min = std::numeric_limits<T>::max();
            max = std::numeric_limits<T>::min();
        }

        Range(T a, T b)
        {
            min = std::min(a,b);
            max = std::max(a,b);
        }

        bool IsValid(void) const
        {
            return max > min;
        }

        bool Intersects(const Range &other) const
        {
            return max > other.min && min < other.max;
        }

        bool Contains(const T other) const
        {
            return other>=min && other<=max;
        }

        Range<T> Inflate(const T padding) const
        {
            return Range<T>(min-padding, max+padding);
        }

        T min;
        T max;
    };
    typedef Range<double> TimeRange;

} // namespace Monitor
} // namespace OVR

#endif
