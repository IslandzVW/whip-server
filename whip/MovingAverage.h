#pragma once

#include <boost/circular_buffer.hpp>

/**
 * Provides calculation of a moving average
 */
class MovingAverage
{
private:
	boost::circular_buffer<int> _buffer;

public:
	MovingAverage(int sampleSize);
	virtual ~MovingAverage();

	/**
	 * Adds a new sample to the moving average
	 */
	void addSample(int sample);

	/**
	 * Returns the current cummulative average
	 */
	int getAverage() const;
};

