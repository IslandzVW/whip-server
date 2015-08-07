#pragma once

class RequestError : public std::runtime_error
{
public:
	RequestError(const std::string& what);
	virtual ~RequestError() throw();
};
