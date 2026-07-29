#include "Shader.h"
#include <GL/glew.h>
#include <fstream>
#include <sstream>
#include <iostream>

namespace Engine
{
    Shader::~Shader()
    {
        if (m_program) glDeleteProgram(m_program);
    }

    std::string Shader::readFile(const std::string& path)
    {
        std::ifstream file(path);
        std::stringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    unsigned int Shader::compile(unsigned int type, const std::string& source)
    {
        unsigned int shader = glCreateShader(type);
        const char* src = source.c_str();
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        int success;
        char log[1024];
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            glGetShaderInfoLog(shader, 1024, nullptr, log);
            std::cerr << "[Shader] Compile error in " << (type == GL_VERTEX_SHADER ? "VERTEX" : "FRAGMENT") << ":\n" << log << std::endl;
        }

        return shader;
    }

    bool Shader::load(const std::string& vertexPath, const std::string& fragmentPath)
    {
        std::string vert = readFile(vertexPath);
        std::string frag = readFile(fragmentPath);
        
        if (vert.empty() || frag.empty())
        {
            std::cerr << "[Shader] Failed to read shader files: " << vertexPath << " / " << fragmentPath << std::endl;
            return false;
        }

        unsigned int vs = compile(GL_VERTEX_SHADER, vert);
        unsigned int fs = compile(GL_FRAGMENT_SHADER, frag);

        m_program = glCreateProgram();
        glAttachShader(m_program, vs);
        glAttachShader(m_program, fs);
        glLinkProgram(m_program);

        int success;
        char log[1024];
        glGetProgramiv(m_program, GL_LINK_STATUS, &success);
        if (!success)
        {
            glGetProgramInfoLog(m_program, 1024, nullptr, log);
            std::cerr << "[Shader] Program link error:\n" << log << std::endl;
            return false;
        }

        glDeleteShader(vs);
        glDeleteShader(fs);
        return true;
    }

    void Shader::use() const
    {
        glUseProgram(m_program);
    }

    int Shader::getUniformLocation(const std::string& name)
    {
        if (m_uniformCache.find(name) == m_uniformCache.end())
        {
            m_uniformCache[name] = glGetUniformLocation(m_program, name.c_str());
        }
        return m_uniformCache[name];
    }

    void Shader::setMat4(const std::string& name, const glm::mat4& value)
    {
        glUniformMatrix4fv(getUniformLocation(name), 1, GL_FALSE, &value[0][0]);
    }

    void Shader::setMat3(const std::string& name, const glm::mat3& value)
    {
        glUniformMatrix3fv(getUniformLocation(name), 1, GL_FALSE, &value[0][0]);
    }

    void Shader::setVec3(const std::string& name, const glm::vec3& value)
    {
        glUniform3fv(getUniformLocation(name), 1, &value[0]);
    }

    void Shader::setFloat(const std::string& name, float value)
    {
        glUniform1f(getUniformLocation(name), value);
    }

    void Shader::setInt(const std::string& name, int value)
    {
        glUniform1i(getUniformLocation(name), value);
    }
}
