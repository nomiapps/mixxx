#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <utility>
#include <vector>

// AI-generated explanation begins.
// Fit relative clock coordinates and periodically rebase them to avoid loss of
// precision after long sessions. Storage is reserved before audio processing.
// AI-generated explanation ends.
class HostTimeFilter {
  public:
    static constexpr std::chrono::microseconds kInvalidHostTime = std::chrono::microseconds::min();

    explicit HostTimeFilter(std::size_t numPoints)
            : m_numPoints(std::max<std::size_t>(numPoints, 2)) {
        m_points.reserve(m_numPoints);
    }

    void clear() {
        m_points.clear();
        m_index = 0;
        resetSums();
    }

    void insertTimePoint(double auxiliaryTime, std::chrono::microseconds hostTime) {
        const auto point = std::make_pair(auxiliaryTime, static_cast<double>(hostTime.count()));
        if (m_points.empty()) {
            m_origin = point;
        }
        if (m_points.size() == m_numPoints) {
            accumulate(m_points[m_index], -1.0);
            m_points[m_index] = point;
        } else {
            m_points.push_back(point);
        }
        accumulate(point, 1.0);
        m_index = (m_index + 1) % m_numPoints;
        if (m_index == 0) {
            m_origin = m_points.front();
            resetSums();
            for (const auto& stored : m_points) {
                accumulate(stored, 1.0);
            }
        }
    }

    std::chrono::microseconds calcHostTime(double auxiliaryTime) const {
        if (m_points.size() < 2) {
            return kInvalidHostTime;
        }
        const double n = static_cast<double>(m_points.size());
        const double denominator = n * m_sumAuxSquared - m_sumAux * m_sumAux;
        if (denominator <= 0.0) {
            return kInvalidHostTime;
        }
        const double slope = (n * m_sumAuxByHst - m_sumAux * m_sumHst) / denominator;
        const double result = m_origin.second + m_sumHst / n +
                slope * (auxiliaryTime - m_origin.first - m_sumAux / n);
        if (!std::isfinite(result)) {
            return kInvalidHostTime;
        }
        return std::chrono::microseconds(std::llround(result));
    }

  private:
    void resetSums() {
        m_sumAux = 0.0;
        m_sumHst = 0.0;
        m_sumAuxByHst = 0.0;
        m_sumAuxSquared = 0.0;
    }

    void accumulate(const std::pair<double, double>& point, double sign) {
        const double x = point.first - m_origin.first;
        const double y = point.second - m_origin.second;
        m_sumAux += sign * x;
        m_sumHst += sign * y;
        m_sumAuxByHst += sign * x * y;
        m_sumAuxSquared += sign * x * x;
    }

    const std::size_t m_numPoints;
    std::size_t m_index = 0;
    std::vector<std::pair<double, double>> m_points;
    std::pair<double, double> m_origin;
    double m_sumAux = 0.0;
    double m_sumHst = 0.0;
    double m_sumAuxByHst = 0.0;
    double m_sumAuxSquared = 0.0;
};
