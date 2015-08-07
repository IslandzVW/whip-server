#pragma once

#include <stdexcept>

class EndOfFileError : public std::runtime_error
{
public:
	EndOfFileError(const std::string& message);
	virtual ~EndOfFileError() throw();
};
