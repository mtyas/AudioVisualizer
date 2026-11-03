// Shadertoy-Compatible Audio-Reactive Neon Rings (MIT / CC0)
// Load via "Load .glsl File..." in Tab 1 -> 2D Shader / GLSL
void mainImage(out vec4 fragColor, in vec2 fragCoord)
{
    vec2 uv = (fragCoord - 0.5 * iResolution.xy) / iResolution.y;
    vec3 col = vec3(0.0);
    for (int i = 0; i < 8; ++i)
    {
        float fi = float(i);
        float band = u_bands[i * 2];
        float rad = 0.12 + fi * 0.08 + band * 0.18;
        float d = abs(length(uv) - rad);
        vec3 ringCol = 0.5 + 0.5 * cos(iTime + fi * 0.6 + vec3(0.0, 2.1, 4.2) + u_centroid * 4.0);
        col += ringCol * (0.005 * (1.0 + band * 4.0)) / (d + 0.004);
    }
    vec3 prev = texture(iChannel0, (fragCoord / iResolution.xy - 0.5) * 0.985 + 0.5).rgb * u_feedback;
    fragColor = vec4(max(col, prev), 1.0);
}
