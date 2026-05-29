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

    // --- orbiting planet: solid sphere + soft glow ring on a circular orbit ---
    // The orbit sweeps from the top-right down to the bottom-centre and back as
    // a closed loop: trUv and bcUv are opposite points on the circle, aUv spans
    // between them, and bUv (its perpendicular) gives the circular bulge.
    vec2 pc = vec2(uv.x * aspect, uv.y);
    float ang = uTime * 0.07;
    vec2 trUv = vec2(0.82, 0.16);  // top-right
    vec2 bcUv = vec2(0.50, 0.85);  // bottom-centre
    vec2 ctrUv = (trUv + bcUv) * 0.5;
    vec2 aUv = (trUv - bcUv) * 0.5;
    vec2 bUv = vec2(-aUv.y, aUv.x);
    vec2 planetUv = ctrUv + cos(ang) * aUv + sin(ang) * bUv;
    vec2 planet = vec2(planetUv.x * aspect, planetUv.y);
    float pd = length(pc - planet);
    float pr = 0.07;
    // soft outer glow ring — kept as before, drawn first so the solid body
    // sits on top and the glow reads as a ring around it.
    float halo = smoothstep(pr * 2.4, pr, pd) * 0.06;
    col += vec3(0.45, 0.85, 0.78) * halo;
    // solid planet body: filled disc with a crisp anti-aliased edge.
    float body = smoothstep(pr, pr - 0.006, pd);
    // Shade it as a sphere using a real surface normal. This is smooth
    // everywhere — including the centre, where the normal points straight at
    // the viewer — so there is no angular singularity / bright spike.
    vec2 rel = (pc - planet) / pr;                 // -1..1 across the disc
    float z = sqrt(max(0.0, 1.0 - dot(rel, rel))); // hemisphere height
    vec3 normal = vec3(rel, z);
    vec3 lightDir = normalize(vec3(-0.45, -0.55, 0.85));
    float lambert = clamp(dot(normal, lightDir), 0.0, 1.0);
    float shade = 0.45 + 0.55 * lambert;
    vec3 planetCol = mix(vec3(0.22, 0.48, 0.44), vec3(0.58, 0.95, 0.86), shade);
    col = mix(col, planetCol, body);

    // --- rounded-rect alpha mask (keeps the card's 16px corners) ---
    float d = sdRoundRect(local - size * 0.5, size * 0.5, uRadius);
    float alpha = smoothstep(1.0, -1.0, d);

    finalColor = vec4(col, alpha);
}
