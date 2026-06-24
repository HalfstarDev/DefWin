#version 140

in mediump vec2 var_texcoord0;

out vec4 out_fragColor;

uniform mediump sampler2D tex0;

uniform fs_uniforms
{
    mediump vec4 clear_color;
};

const float STEP = 1.0 / 255.0;      // Step to the next color (0-255)
const float EPSILON = STEP * 0.501;  // Difference from color that could round to other color

void main()
{
    vec4 color = texture(tex0, var_texcoord0);

    if (color.a < STEP) {
        out_fragColor = vec4(clear_color.rgb, 1.0);
        return;
    }

    vec3 diff = abs(color.rgb - clear_color.rgb);
    if (diff.r < EPSILON && diff.g < EPSILON && diff.b < EPSILON) {
        if (color.r < 1.0 && color.g < 1.0 && color.b < 1.0) {
            color.rgb = clamp(color.rgb + vec3(STEP), 0.0, 1.0);
        } else {
            color.rgb = clamp(color.rgb - vec3(STEP), 0.0, 1.0);
        }
    }

    out_fragColor = color;
}