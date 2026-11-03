/*{
  "DESCRIPTION": "ISF Hyper-Tunnel with Audio Envelope & Spectrum Reactivity",
  "CREDIT": "AudioVisualizer Open Content Pack (CC0 / MIT)",
  "ISFVSN": "2.0",
  "CATEGORIES": [ "Generator", "AudioVisualizer" ],
  "INPUTS": [
    { "NAME": "u_env", "TYPE": "float", "DEFAULT": 0.4, "MIN": 0.0, "MAX": 1.0 },
    { "NAME": "u_transient", "TYPE": "float", "DEFAULT": 0.2, "MIN": 0.0, "MAX": 1.0 }
  ]
}*/

void main()
{
    vec2 uv = isf_FragNormCoord * 2.0 - 1.0;
    uv.x *= RENDERSIZE.x / max(RENDERSIZE.y, 1.0);
    float r = length(uv) + 1e-4;
    float a = atan(uv.y, uv.x);
    float z = 1.5 / r + TIME * (1.8 + u_env * 2.5);
    float pattern = sin(z * 6.0) * cos(a * 6.0 + sin(z * 1.5 + TIME));
    float glow = (0.04 + u_transient * 0.08) / (abs(pattern) + 0.03);
    vec3 col = (0.5 + 0.5 * cos(z * 0.4 + vec3(0.0, 2.0, 4.0))) * glow * smoothstep(0.02, 0.25, r);
    gl_FragColor = vec4(col, 1.0);
}
