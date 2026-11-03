#pragma once
#include <juce_opengl/juce_opengl.h>
#include <memory>
#include <unordered_map>

// Drop-in replacement for std::unique_ptr<juce::OpenGLShaderProgram> that caches uniform and
// attribute locations. JUCE's OpenGLShaderProgram::setUniform calls glGetUniformLocation on
// every call (~110 driver round-trips per frame here); this resolves each name once.
//
// The cache is keyed on the name *pointer*, so names must be string literals or otherwise have
// stable storage (true for every call site in Renderer.cpp).
class ShaderProg
{
public:
    ShaderProg() = default;
    ShaderProg (std::unique_ptr<juce::OpenGLShaderProgram> p) : prog (std::move (p)) {}

    ShaderProg& operator= (std::unique_ptr<juce::OpenGLShaderProgram> p)
    {
        prog = std::move (p);
        uniforms.clear();
        attribs.clear();
        return *this;
    }

    void reset() { *this = nullptr; }

    explicit operator bool() const noexcept { return prog != nullptr; }
    ShaderProg* operator->() noexcept { return this; }

    void use() const noexcept                         { prog->use(); }
    GLuint getProgramID() const noexcept              { return prog->getProgramID(); }

    GLint loc (const char* name)
    {
        const auto it = uniforms.find (name);
        if (it != uniforms.end())
            return it->second;
        const GLint l = juce::gl::glGetUniformLocation (prog->getProgramID(), name);
        uniforms.emplace (name, l);
        return l;
    }

    GLint attrib (const char* name)
    {
        const auto it = attribs.find (name);
        if (it != attribs.end())
            return it->second;
        const GLint l = juce::gl::glGetAttribLocation (prog->getProgramID(), name);
        attribs.emplace (name, l);
        return l;
    }

    // Same overload set as juce::OpenGLShaderProgram so existing call sites compile unchanged.
    // Location -1 (optimised-out / absent uniform) is a silent no-op in GL, matching JUCE.
    void setUniform (const char* n, GLfloat a)                              { juce::gl::glUniform1f (loc (n), a); }
    void setUniform (const char* n, GLint a)                                { juce::gl::glUniform1i (loc (n), a); }
    void setUniform (const char* n, GLfloat a, GLfloat b)                   { juce::gl::glUniform2f (loc (n), a, b); }
    void setUniform (const char* n, GLfloat a, GLfloat b, GLfloat c)        { juce::gl::glUniform3f (loc (n), a, b, c); }
    void setUniform (const char* n, GLfloat a, GLfloat b, GLfloat c, GLfloat d) { juce::gl::glUniform4f (loc (n), a, b, c, d); }
    void setUniform (const char* n, const GLfloat* v, GLsizei count)        { juce::gl::glUniform1fv (loc (n), count, v); }
    void setUniformMat4 (const char* n, const GLfloat* m, GLint count, GLboolean transpose)
    {
        juce::gl::glUniformMatrix4fv (loc (n), count, transpose, m);
    }

private:
    std::unique_ptr<juce::OpenGLShaderProgram> prog;
    std::unordered_map<const char*, GLint> uniforms, attribs;
};
