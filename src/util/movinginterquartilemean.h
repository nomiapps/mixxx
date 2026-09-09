#pragma once

#include <algorithm>
#include <vector>

// Truncated Interquartile mean

// TruncatedIQM keeps an ordered list with the last n (_capacity_)
// input doubles and calculates the mean discarding the lowest 25%
// and the highest 25% values in order to reduce sensitivity to outliers.
//
// http://en.wikipedia.org/wiki/Interquartile_mean
class MovingInterquartileMean {
  public:
    // Constructs an empty MovingTruncatedIQM.
    MovingInterquartileMean(std::size_t listLength)
            : m_dMean(0.0),
              m_bChanged(true) {
        m_list.reserve(std::max<std::size_t>(listLength, 1));
        m_history.resize(m_list.capacity());
    }

    // Inserts value to the list and returns the new truncated mean.
    double insert(double value);
    // Empty the list.
    void clear();
    // Returns the current truncated mean. Input list must not be empty.
    double mean();
    // Returns how many values have been input.
    int size() const {
        return static_cast<int>(m_list.size());
    }

  private:
    double calcMean() const;
    // The list keeps input doubles ordered by value.
    std::vector<double> m_list;
    // AI-generated explanation begins.
    // A fixed ring avoids queue allocations in the audio callback.
    // AI-generated explanation ends.
    std::vector<double> m_history;
    std::size_t m_historyIndex = 0;
    double m_dMean;

    // sum() checks this to know if it has to recalculate the mean.
    bool m_bChanged;
};
