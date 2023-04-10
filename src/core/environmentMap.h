#pragma once
#include <string>
#include "logger.h"

NAMESPACE_BEGIN(nagi)

class EnvironmentMap
{
public:
	EnvironmentMap() :width(0), height(0), img(nullptr), cdf(nullptr) {}
	~EnvironmentMap();

	void BuildCDF();
	bool LoadEnvMap(std::string& filename);

	int width, height;
	float totalSum;
	float* img;
	float* cdf;
};

NAMESPACE_END(nagi)