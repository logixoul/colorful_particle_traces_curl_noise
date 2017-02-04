varying vec3 pos;
varying vec3 normal;
varying vec3 pos2;
attribute vec3 attrib_pos0;
varying vec3 pos0;
varying vec2 vTexCoord;

void main()
{
	gl_Position = gl_ModelViewProjectionMatrix * gl_Vertex;
	pos2 = (gl_ModelViewMatrix * gl_Vertex).xyz;
	pos = gl_Vertex.xyz;
	vTexCoord = gl_MultiTexCoord0.xy;
	normal = gl_Normal.xyz;
	pos0 = attrib_pos0;
}