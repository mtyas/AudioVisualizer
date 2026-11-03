#pragma once
#include <juce_core/juce_core.h>

// Translates ISF (Interactive Shader Format) and legacy GLSL shaders into our
// GLSL 1.50 Shadertoy-style `mainImage(out vec4, in vec2)` layer body.
namespace isf
{
inline juce::String translateShaderIfISF (const juce::String& rawSource)
{
    // If it already defines mainImage, return as-is
    if (rawSource.contains ("mainImage"))
        return rawSource;

    juce::String src = rawSource;
    juce::String uniformDecls;

    // Parse optional ISF JSON metadata block /*{ ... }*/
    const int jsonStart = src.indexOf ("/*{");
    const int jsonEnd   = src.indexOf ("}*/");
    if (jsonStart >= 0 && jsonEnd > jsonStart)
    {
        const auto jsonText = src.substring (jsonStart + 2, jsonEnd + 1);
        src = src.substring (0, jsonStart) + src.substring (jsonEnd + 3);

        const auto meta = juce::JSON::parse (jsonText);
        if (auto* inputs = meta.getProperty ("INPUTS", juce::var()).getArray())
        {
            for (const auto& inp : *inputs)
            {
                const auto name = inp.getProperty ("NAME", "").toString();
                const auto type = inp.getProperty ("TYPE", "float").toString().toLowerCase();
                const auto def  = inp.getProperty ("DEFAULT", 0.5);
                if (name.isEmpty()) continue;

                if (type == "float")
                    uniformDecls << "uniform float " << name << ";\n";
                else if (type == "bool")
                    uniformDecls << "const bool " << name << " = true;\n";
                else if (type == "color")
                    uniformDecls << "const vec4 " << name << " = vec4(0.2, 0.8, 1.0, 1.0);\n";
                else if (type == "point2d")
                    uniformDecls << "const vec2 " << name << " = vec2(0.5, 0.5);\n";
                else if (type == "image")
                    uniformDecls << "#define " << name << " iChannel1\n";
                juce::ignoreUnused (def);
            }
        }
    }

    // ISF built-in macros & compatibility bridge
    juce::String isfHeader;
    isfHeader
        << "// --- ISF Compatibility Bridge ---\n"
        << "#define TIME iTime\n"
        << "#define TIMEDELTA iTimeDelta\n"
        << "#define RENDERSIZE (iResolution.xy)\n"
        << "#define FRAMEINDEX iFrame\n"
        << "#define isf_FragNormCoord (gl_FragCoord.xy / iResolution.xy)\n"
        << "#define IMG_NORM_PIXEL(tex, coord) texture(tex, coord)\n"
        << "#define IMG_PIXEL(tex, coord) texture(tex, (coord) / iResolution.xy)\n"
        << uniformDecls;

    // Replace `void main()` with `void _isf_main()` and `gl_FragColor` with `_isf_FragColor`
    src = src.replace ("gl_FragColor", "_isf_FragColor");
    src = src.replace ("void main()", "void _isf_main()");
    src = src.replace ("void main(void)", "void _isf_main()");

    juce::String wrapped;
    wrapped
        << isfHeader
        << "vec4 _isf_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
        << src << "\n"
        << "void mainImage(out vec4 o, in vec2 fc)\n"
        << "{\n"
        << "    _isf_main();\n"
        << "    o = _isf_FragColor;\n"
        << "}\n";
    return wrapped;
}
} // namespace isf
