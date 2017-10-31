// tex=src
// tex2=vectorField
vec3 sum=vec3(0.0);
int r=3;
for(int x=-r;x<=r;x++) {
	for(int y=-r;y<=r;y++) {
		vec2 offset = vec2(ivec2(x, y));
		vec2 nbtc=tc+tsize*offset;
		vec2 velocity=fetch3(tex2,nbtc).xy;
		vec2 toVecTarget = offset+velocity;
		float xContrib=1.0-abs(toVecTarget.x);
		float yContrib=1.0-abs(toVecTarget.y);
		if(xContrib > 0.0 && yContrib > 0.0) {
			sum+=xContrib*yContrib*fetch3(tex,nbtc);
		}
	}
}
_out=sum;
