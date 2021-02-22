out vec4 fColor;
in vec2 vCoord;
layout(binding=0)uniform sampler2D tex[8];
uniform float offset = 0;
uniform float shift  = 0;
uniform int   mode   = 0;
void main(){
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
  if(mode==1)
    fColor = vec4(texture(tex[uint(clamp(shift*8,0,7))],vCoord));

}
