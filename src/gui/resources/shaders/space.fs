#version 330

// Inputs provided by raylib's default vertex shader.
in vec2 fragTexCoord;
in vec4 fragColor;

out vec4 finalColor;

uniform float uTime;        // seconds
uniform vec2  uResolution;  // window size in px
uniform vec4  uCardRect;    // card x, y, w, h (top-left origin, screen px)
uniform float uRadius;      // corner radius in px
uniform vec3  uColorTop;    // teal gradient (top)
uniform vec3  uColorDeep;   // teal gradient (bottom, darker)

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Signed distance to a rounded rectangle centered at origin.
// p: point relative to center, b: half-size, r: corner radius.
float sdRoundRect(vec2 p, vec2 b, float r) {
    vec2 q = abs(p) - b + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

void main() {
    // gl_FragCoord is bottom-left origin; convert to top-left screen coords.
    vec2 frag = vec2(gl_FragCoord.x, uResolution.y - gl_FragCoord.y);

    vec2 local = frag - uCardRect.xy;   // 0..w, 0..h within the card
    vec2 size  = uCardRect.zw;
    vec2 uv    = local / size;          // 0..1 across the card
    float aspect = size.x / size.y;

    // --- base vertical teal gradient ---
    vec3 col = mix(uColorTop, uColorDeep, clamp(uv.y, 0.0, 1.0));

    // --- procedural starfield (slow drift + twinkle) ---
    vec2 sp = vec2(uv.x * aspect, uv.y);
    sp += vec2(uTime * 0.012, uTime * 0.018);
    vec2 gv   = sp * 22.0;
    vec2 cell = floor(gv);
    vec2 f    = fract(gv);
    float rnd = hash21(cell);
    if (rnd > 0.55) {
        vec2 starPos = vec2(hash21(cell + 1.7), hash21(cell + 4.3));
        float d = length(f - starPos);
        float twinkle = 0.5 + 0.5 * sin(uTime * 2.0 + rnd * 6.2831);
        float bright = smoothstep(0.06, 0.0, d) * (0.35 + 0.65 * rnd) * twinkle;
        col += vec3(0.75, 0.92, 0.85) * bright * 0.6;
    }

    // --- orbiting planet (top-right): one soft radial orb ---
    // No hard body disc and no separate ring — a single smooth falloff from a
    // gentle core to nothing, so there is no sharp dot in the centre.
    vec2 pc = vec2(uv.x * aspect, uv.y);
    float ang = uTime * 0.25;
    vec2 orbitC = vec2(0.82 * aspect, 0.16);
    vec2 planet = orbitC + vec2(cos(ang) * 0.08 * aspect, sin(ang) * 0.035);
    float pd = length(pc - planet);
    float pr = 0.07;
    // soft outer glow ring — kept as before, drawn first so the solid body
    // sits on top and the glow reads as a ring around it.
    float halo = smoothstep(pr * 2.4, pr, pd) * 0.06;
    col += vec3(0.45, 0.85, 0.78) * halo;
    // solid planet body: filled disc with a crisp anti-aliased edge and a
    // gentle directional terminator for a little depth.
    float body = smoothstep(pr, pr - 0.006, pd);
    vec2 ldir = normalize(vec2(-0.4, -0.5));
    float shade = clamp(dot(normalize(pc - planet + 1e-5), ldir) * 0.5 + 0.5,
                        0.0, 1.0);
    vec3 planetCol = mix(vec3(0.28, 0.58, 0.52), vec3(0.58, 0.95, 0.86), shade);
    col = mix(col, planetCol, body);

    // --- rounded-rect alpha mask (keeps the card's 16px corners) ---
    float d = sdRoundRect(local - size * 0.5, size * 0.5, uRadius);
    float alpha = smoothstep(1.0, -1.0, d);

    finalColor = vec4(col, alpha);
}
