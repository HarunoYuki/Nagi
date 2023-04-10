#pragma once
#include "glad.h"
#include "logger.h"

NAMESPACE_BEGIN(nagi)

class Program;

class Quad
{
public:
	Quad();
	void Draw(Program* id);

private:
	GLuint vao, vbo;
};

NAMESPACE_END(nagi)