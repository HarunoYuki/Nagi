#pragma once
#include <string>
#include "logger.h"
#include "glad.h"

NAMESPACE_BEGIN(nagi)

struct ShaderSource
{
	std::string src;
	std::string path;
};

class Shader
{
public:
	Shader(ShaderSource& shaderSource, GLenum shaderType);

	static ShaderSource LoadFullShaderCode(std::string& path, std::string includeIndentifier = "#include ");
	GLuint getShader() const { return object; }

private:
	GLuint object;
};

NAMESPACE_END(nagi)