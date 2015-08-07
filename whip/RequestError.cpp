#include "StdAfx.h"
#include "RequestError.h"

RequestError::RequestError(const std::string& what)
: std::runtime_error(what)
{
}

RequestError::~RequestError() throw()
{
}
