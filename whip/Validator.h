#pragma once

#include <string>

class Validator
{
public:
	const static short UUID_LEN = 32;

private:
	Validator();
	~Validator();

public:
	static bool IsValidUUID(const std::string& uuid); 
};
