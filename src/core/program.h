#pragma once
#include <vector>
#include "glad.h"
#include "logger.h"

NAMESPACE_BEGIN(nagi)

class Shader;

class Program
{
public:
	Program(const std::vector<Shader>& shaders);
	~Program() { glDeleteProgram(object); }

	void use() { glUseProgram(object); }
	void stop() { glUseProgram(0); }

	GLuint GetProgram() const { return object; }

private:
	GLuint object;
};

NAMESPACE_END(nagi)