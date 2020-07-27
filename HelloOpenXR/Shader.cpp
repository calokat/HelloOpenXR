#include "Shader.h"

Shader::Shader(std::string pathArg, GLint typeArg)
{
	shaderType = typeArg;
	id = glCreateShader(shaderType);
	path = pathArg;
	stream.open(path, std::ios::binary);
	if (!stream.is_open())
	{
		std::cout << "No file found";
		return;
	}
	stream.seekg(0, std::ios::end);
	int shaderLength = (int)stream.tellg();
	char* shaderSource = new char[shaderLength + 1];
	stream.seekg(0, std::ios::beg);
	stream.read(shaderSource, shaderLength);
	shaderSource[shaderLength] = '\0';
	stream.close();
	glShaderSource(id, 1, &shaderSource, 0);
	/*glCompileShader(vShader);
	int checkStatus;
	glGetShaderiv(vShader, GL_COMPILE_STATUS, &checkStatus);
	if (checkStatus == 0)
	{
		GLint length = 0;
		glGetShaderiv(vShader, GL_INFO_LOG_LENGTH, &length);
		GLchar* log = new char[length + 1];
		glGetShaderInfoLog(vShader, length, 0, log);
		std::cout << log << '/n';
		glDeleteShader(vShader);
		delete[] log;

		std::cout << "Shader compilation failed" << std::endl;
		return;
	}*/

	delete[] shaderSource;
}

void Shader::Compile()
{
	glCompileShader(id);
	int checkStatus;
	glGetShaderiv(id, GL_COMPILE_STATUS, &checkStatus);
	if (checkStatus == 0)
	{
		GLint length = 0;
		glGetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
		GLchar* log = new char[length + 1];
		glGetShaderInfoLog(id, length, 0, log);
		std::cout << log << '/n';
		glDeleteShader(id);
		delete[] log;

		std::cout << "Shader compilation failed" << std::endl;
		return;
	}

}

GLint Shader::GetId()
{
	return id;
}
