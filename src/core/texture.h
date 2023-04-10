#pragma once
#include <vector>
#include <string>
#include "logger.h"

NAMESPACE_BEGIN(nagi)


class Texture
{
public:
	Texture() :width(0), height(0), components(0) {}
	Texture(std::string& name, unsigned char* data, int w, int h, int c);

	bool LoadTexture(std::string& filename);

	int width, height, components;
	std::vector<unsigned char> texData;
	std::string name;
};

NAMESPACE_END(nagi)