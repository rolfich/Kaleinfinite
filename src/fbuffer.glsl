#if defined(VERTEX)
uniform mat4 Projection;
uniform mat4 View;

in vec3 VertexPosition;
in vec3 VertexNormal;
in vec2 VertexTexCoord;

out vData {
	vec2 uv;
    vec3 normal;
    vec3 position;
}vertex;

void main(void) {
	gl_Position = Projection * View * vec4(VertexPosition, 1.0);
	vertex.normal = VertexNormal;
	vertex.uv = VertexTexCoord;
	vertex.position = VertexPosition;
}

#endif

#if defined(FRAGMENT)

uniform sampler2D Texture;
uniform float Time;
 
in vData {
	vec2 uv;
    vec3 normal;
    vec3 position;
} frag;

out vec4 Color;
out vec4  Normal;

void main(void) {

    vec3 texture = texture(Texture, frag.uv).rgb;
    Normal = vec4(frag.normal, 1.0);
    if(texelFetch(Texture, ivec2(gl_FragCoord), 0).rgb != vec3(0., 0., 0.)){
        Color = vec4(texture, 1.0) + vec4(0.2*cos(Time), 0.2*sin(Time), 0.2*cos(-Time), 1.0);
    }
}
#endif
