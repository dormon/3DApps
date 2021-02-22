out vec4 fColor;
in vec2 vCoord;
layout(binding=0)uniform sampler2D tex[8];
uniform float inOffset = 0;
uniform float inShift  = 0;
uniform int   mode   = 0;
void main(){
  float shift = inShift / 1000.f;
  float offset = inOffset / 1000.f;
  vec4 meanColor = vec4(0);

  for(int i=0;i<8;++i)
    meanColor += texture(tex[i],vCoord+vec2(offset*i-shift,0));
  meanColor/=8;
  vec4 varianceColor = vec4(0);
  for(int i=0;i<8;++i){
    vec4 diff = meanColor-texture(tex[i],vCoord+vec2(offset*i-shift,0));
    varianceColor += diff*diff;
  }
  varianceColor /= 8;
  if(mode==0)
    fColor = vec4(meanColor);
  if(mode==1)
    fColor = vec4(varianceColor);
  if(mode==2){
    vec4 a = texture(tex[uint(clamp(shift*8  ,0,7))],vCoord);
    vec4 b = texture(tex[uint(clamp(shift*8+1,0,7))],vCoord);
    float t = fract(shift*8);
    fColor = vec4(a*(1-t)+b*t);
  }

}
