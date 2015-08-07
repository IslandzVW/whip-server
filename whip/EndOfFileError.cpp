#include "StdAfx.h"
#include "EndOfFileError.h"

EndOfFileError::EndOfFileError(const std::string& message)
: std::runtime_error(message)
{
}

EndOfFileError::~EndOfFileError() throw()
{
}
