#include "StdAfx.h"
#include "MovingAverage.h"

#include <numeric>


MovingAverage::MovingAverage(int sampleSize)
	: _buffer(sampleSize)
{
}


MovingAverage::~MovingAverage()
{
}

void MovingAverage::addSample(int sample)
{
	_buffer.push_back(sample);
}

int MovingAverage::getAverage() const
{
	if (_buffer.size() > 0)
	{
		int sum = std::accumulate(_buffer.begin(), _buffer.end(), 0);
		return sum / (int)_buffer.size();
	}
	else
	{
		return 0;
	}
}