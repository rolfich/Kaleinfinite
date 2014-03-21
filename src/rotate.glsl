#if defined(VERTEX)
uniform mat3 Rotation;
uniform mat4 View;
uniform mat4 Projection;
uniform vec3 CameraPosition;
in vec3 VertexPosition;
in vec2 VertexTexCoord;

out vData {
	vec2 uv;
}vertex;

void main(void) {
    vec3 position = vec3(VertexPosition.x, CameraPosition.y -7, VertexPosition.z);
    gl_Position = normalize(Projection * View * vec4(Rotation*position.xyz, 1.0));
    vertex.uv = VertexTexCoord;
}

#endif

#if defined(FRAGMENT)

uniform sampler2D FBOTexture;

in vData {
	vec2 uv;
} frag;

out vec4 Color;

void main(void) {

    vec3 texture = texture(FBOTexture, frag.uv).rgb;
    Color = vec4(texture, 1.0);

}
#endif
