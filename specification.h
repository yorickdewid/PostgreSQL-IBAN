#pragma once
#include <string>

class Specification {
public:
	Specification(std::string countryCode, int length, std::string structure, std::string example) :
		countryCode(countryCode),
		length(length),
		structure(structure),
		example(example)
	{};
	std::string countryCode;
	int length;
	std::string structure;
	std::string example;
};
