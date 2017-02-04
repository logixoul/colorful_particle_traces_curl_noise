uniform sampler2D tex;
uniform vec2 mouse;
varying vec3 pos;
varying vec3 normal;
uniform float time;
varying vec3 pos2;
uniform vec2 lightOrbitCenter;
varying vec2 vTexCoord;

vec3 _out = vec3(0.0);
uniform vec2 viewportSize;

void light(vec3 L, vec3 color)
{
	vec3 L2 = L-pos;
	vec3 nN = normalize(normal);
	vec3 V = pos-(gl_ModelViewMatrixInverse*vec4(0.0,0.0,0.0,1.0)).xyz;
	float _dot = dot(normalize(reflect(V, nN)), normalize(L2));
	float _dot2 = dot(nN, normalize(L2));
    //float att = 1.0 / pow(length(L2), 2.0);
    float att = .0005;
	_out += 1.0 * pow(max(0.0, _dot), 50.0) * color * att;
	_out += .25 * max(0.0, _dot2) * color * att;
}

float s1(float f) { return sin(f) * .5 + .5; }
float c1(float f) { return cos(f) * .5 + .5; }
float clamp01(float f) { return max(0.0, min(1.0, f)); }
void main()
{
    vec3 glowColor = texture2D(tex, vTexCoord).rgb;
    glowColor *= 0.05;
	float twopi_=3.14 * 2.0;
	float t = time * 1.0;
	light(vec3(lightOrbitCenter.x + 70.0*sin(t+twopi_*0.0/3.0), lightOrbitCenter.y + 100.0, 50.0), 2000.0*vec3(1.0, 1.0, 1.0).xyz);
    light(vec3(lightOrbitCenter.x + 70.0*sin(t+twopi_*1.0/3.0), lightOrbitCenter.y + 100.0, 50.0), 2000.0*vec3(1.0, 1.0, 1.0).xyz);
	_out += glowColor - .25*0.0;
	_out = max(_out, vec3(0.0));
	//_out /= _out + vec3(1.0);
	
	//_out = pow(_out, vec3(1.0 / 2.2));
	gl_FragColor = vec4(_out, 1.0);
}