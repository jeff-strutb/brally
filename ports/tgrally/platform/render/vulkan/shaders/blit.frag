#version 450
/* blit.frag: the frame, bilinear, through the VI's gamma when it is on */
layout(push_constant) uniform P { vec4 r; int gamma; } p;
layout(set = 0, binding = 0) uniform sampler2D frame;
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_col;
void main()
{
    vec4 c = texture(frame, v_uv);
    if (p.gamma != 0)
        c.rgb = sqrt(c.rgb);       /* the VI's gamma: the square root */
    o_col = c;
}
