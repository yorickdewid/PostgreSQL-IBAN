#pragma once
#include <memory>
#include <map>
#include "specification.h"

class Validate {
  public:
	Validate();
	~Validate();
	bool isValid(std::string arg);
	void addSpecification(Specification* specPtr);
	void setSelectedSpecification(std::string countryCode);

	std::map<std::string, Specification*> specifications;
	Specification* selectedSpec;
};
