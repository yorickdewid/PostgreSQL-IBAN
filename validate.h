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

	std::map<std::string, Specification*> specifications;
};
