#include "program.h"
#include "shader.h"

NAMESPACE_BEGIN(nagi)

Program::Program(const std::vector<Shader>& shaders)
{
	object = glCreateProgram();
	for (size_t i = 0; i < shaders.size(); i++)
		glAttachShader(object, shaders[i].getShader());

	glLinkProgram(object);
	for (size_t i = 0; i < shaders.size(); i++)
		glDetachShader(object, shaders[i].getShader());

	int success;
	char infoLog[1024];
	glGetProgramiv(object, GL_LINK_STATUS, &success);
	if (!success)
	{
		glGetProgramInfoLog(object, 1024, NULL, infoLog);
		throw std::runtime_error(std::string("Shader program Linking error") + infoLog);
		//Error("Shader program Linking error. \n%s", infoLog);	
	}

}

NAMESPACE_END(nagi)