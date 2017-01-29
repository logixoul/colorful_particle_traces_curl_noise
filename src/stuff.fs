float getL(vec3 c) {
	vec3 w = vec3(.22, .71, .07);
	return dot(w, c);
}
float getLPhotoshop(vec3 c) {
	vec3 w = vec3(.30, .59, .11);
	return dot(w, c);
}
float pi=3.14159265;
vec3 hsv_to_rgb(float h, float s, float v)
{
	float c = v * s;
	h = mod((h * 6.0), 6.0);
	float x = c * (1.0 - abs(mod(h, 2.0) - 1.0));
	vec3 color;

	if (0.0 <= h && h < 1.0) {
		color = vec3(c, x, 0.0);
	} else if (1.0 <= h && h < 2.0) {
		color = vec3(x, c, 0.0);
	} else if (2.0 <= h && h < 3.0) {
		color = vec3(0.0, c, x);
	} else if (3.0 <= h && h < 4.0) {
		color = vec3(0.0, x, c);
	} else if (4.0 <= h && h < 5.0) {
		color = vec3(x, 0.0, c);
	} else if (5.0 <= h && h < 6.0) {
		color = vec3(c, 0.0, x);
	} else {
		color = vec3(0.0, 0.0, 0.0);
	}

	color += v - c;

	return color;
};
