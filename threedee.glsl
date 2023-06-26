@module threedee

@vs vs
in vec3 pos_in;
in vec2 uv_in;
out vec3 pos;
out vec2 uv;

uniform vs_params {
	mat4 model;
	mat4 view;
	mat4 projection;
};

void main() {
	pos = pos_in;
	uv = uv_in;
	gl_Position = projection * view * model * vec4(pos_in, 1.0);
}
@end

@fs fs
uniform sampler2D tex;
in vec3 pos;
in vec2 uv;
out vec4 frag_color;

void main() {
	vec4 col = texture(tex, uv);
	if(col.a < 0.5)
	{
		discard;
	}
	else
	{
		frag_color = vec4(col.rgb, 1.0);
	}
}
@end

@program program vs fs