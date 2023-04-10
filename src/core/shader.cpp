#include <fstream>
#include "shader.h"

NAMESPACE_BEGIN(nagi)

Shader::Shader(ShaderSource& shaderCode, GLenum shaderType)
{
	object = glCreateShader(shaderType);
	printf("Compiling shader \"%s\"", shaderCode.path.c_str());
	const GLchar* src = (const GLchar*)shaderCode.src.c_str();
	glShaderSource(object, 1, &src, NULL);
	glCompileShader(object);
	int success;
	char infoLog[1024];
	glGetShaderiv(object, GL_COMPILE_STATUS, &success);
	if (!success)
	{
		glGetShaderInfoLog(object, 1024, NULL, infoLog);
		glDeleteShader(object); object = 0;
		throw std::runtime_error(shaderCode.path + infoLog);
		//Error("Shader compilation error \"%s\".\n%s", shaderCode.path.c_str(), infoLog);
	}
}

ShaderSource Shader::LoadFullShaderCode(std::string& filename, std::string includeIndentifier)
{
	std::ifstream file(filename);
	if (!file.is_open())
		Error("Fail to open shader file \"%s\"", filename.c_str());

	static bool isRecursiveCall = false;
	std::string fullcode;
	std::string lineBuffer;
	while (std::getline(file, lineBuffer))
	{
		// 如果有includeIndentifier，那么用文件路径替换
		if (lineBuffer.find(includeIndentifier) != lineBuffer.npos)
		{
			lineBuffer.erase(0, includeIndentifier.size());

			std::string includeFilePath = filename.substr(0, filename.find_last_of("/\\") + 1) + lineBuffer;

			isRecursiveCall = true;
			fullcode += LoadFullShaderCode(includeFilePath).src;

			continue;
		}

		fullcode += lineBuffer + "\n";
	}

	// 只在完整的shadercode的最后加入终止符
	// 如果是递归调用，那么作为中间code不应该加入终止符
	if (!isRecursiveCall)
		fullcode += '\0';

	file.close();

	return ShaderSource{ fullcode, filename };
}


NAMESPACE_END(nagi)
