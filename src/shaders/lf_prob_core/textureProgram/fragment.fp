out vec4 fColor;
in vec2 vCoord;
layout(binding=0)uniform sampler2D tex[8];
uniform float inOffset = 0;
uniform float inShift  = 0;
uniform int   mode   = 0;
uniform int   inplaceMeanSize = 4;
uniform bool  showMix = false;
uniform float contrast = 100;
uniform float brightness = 0;

//offset = cameraDistance*near/depth

vec4 getColor(int i,vec2 coord,float offset,float shift){
  return texture(tex[i],coord+vec2(offset*i-shift,0));
}

vec4 getMeanColor(float offset,float shift){
  vec4 meanColor = vec4(0);

  for(int i=0;i<8;++i)
    meanColor += getColor(i,vCoord,offset,shift);
    //meanColor += texture(tex[i],vCoord+vec2(offset*i-shift,0));
  meanColor/=8;
  return meanColor;
}

vec4 getVarianceColor(float offset,float shift){
  vec4 meanColor = getMeanColor(offset,shift);
  vec4 varianceColor = vec4(0);
  for(int i=0;i<8;++i){
    vec4 diff = meanColor-texture(tex[i],vCoord+vec2(offset*i-shift,0));
    varianceColor += diff*diff;
  }
  varianceColor /= 8;
  return varianceColor;
}

vec4 getInplaceMeanColor(){
  vec4 meanColor = vec4(0);
  const float pixelSize = 1.f/float(textureSize(tex[0],0).x);
  const int size = inplaceMeanSize;
  for(int i=-size;i<=size;++i)
    meanColor += texture(tex[0],vCoord+vec2(pixelSize*i,0));
  meanColor /= float(2.f*size+1);
  return meanColor;
}

vec4 getInplaceVarianceColor(){
  vec4 meanColor = getInplaceMeanColor();
  const float pixelSize = 1.f/float(textureSize(tex[0],0).x);
  const int size = inplaceMeanSize;
  vec4 varianceColor = vec4(0);
  for(int i=-size;i<=size;++i){
    vec4 diff = meanColor-texture(tex[0],vCoord+vec2(pixelSize*i,0));
    varianceColor += diff*diff;
  }
  varianceColor /= float(2.f*size+1);
  return varianceColor;
}

void presentColor(vec4 color){
  fColor = clamp(color*contrast/100.f+brightness/256.f,vec4(0),vec4(1));
}

uniform float range=0.01f;
uniform float steps=50.f;

vec4 getDepth(){
  float depth=0;
  float inc=2.f*range/steps;
  
  float bestVariance=1e10f;
  float bestFocus=-range;
  for(float f=-range;f<range;f+=inc){
    vec4 mean = vec4(0);
    for(int i=0;i<8;++i)
      mean+=texture(tex[i],vec2(vCoord.x+i*f,vCoord.y));
    mean/=8.f;
    float variance=0.f;
    for(int i=0;i<8;++i)
      variance+=length(texture(tex[i],vec2(vCoord.x+i*f,vCoord.y))-mean)/sqrt(4.f);
    variance/=8.f;
    if(variance<bestVariance){
      bestFocus=f;
      bestVariance=variance;
    }
  }
  return vec4((bestFocus+range)/range/2.f);
}

uniform float stripSize=1.f;

vec4 getDepth2(){
  float depth=0;
  float inc=2.f*range/steps;
  
  float bestVariance=1e10f;
  float bestFocus=-range;
  for(float f=-range;f<range;f+=inc){
    const float pixs = 1.f/textureSize(tex[0],0).x;
    const float pp=pixs*stripSize;
    float globVar=0.f;
    for(float p=-pp;p<pp;p+=pixs){
      vec4 mean = vec4(0);
      for(int i=0;i<8;++i)
        mean+=texture(tex[i],vec2(vCoord.x+i*f+p,vCoord.y));
      mean/=8.f;
      float variance=0.f;
      for(int i=0;i<8;++i)
        variance+=length(texture(tex[i],vec2(vCoord.x+i*f+p,vCoord.y))-mean)/sqrt(4.f);
      variance/=8.f;
      globVar+=variance*((pp-abs(p))/pp);
    }
    if(globVar<bestVariance){
      bestFocus=f;
      bestVariance=globVar;
    }
  }
  return vec4((bestFocus+range)/range/2.f);
}

uniform float pixX = 50;
uniform float pixY = 50;

void main(){
  float shift = inShift / 100.f;
  float offset = inOffset / 5000.f;
  if(mode==0 || showMix){
    presentColor(getMeanColor(offset,shift));

    if(length(vCoord-vec2(pixX/100,pixY/100))<0.001){
      fColor = vec4(1,0,0,1);
      return;
    }
    if(vCoord.y>0.9){
      vec2 nc = vCoord;
      nc.y-=0.9;
      nc.y/=0.1;
      fColor = vec4(nc,0,1);
      vec4 cc = getColor(int(nc.x*8),vec2(pixX/100,pixY/100),offset,shift); 
      fColor = vec4(cc.r>nc.y,cc.g>nc.y,cc.r>nc.y,1);
    }
    return;
  }
  if(mode==1)
    presentColor(getVarianceColor(offset,shift));
  if(mode==2){
    vec4 a = texture(tex[uint(clamp(shift*8  ,0,7))],vCoord);
    vec4 b = texture(tex[uint(clamp(shift*8+1,0,7))],vCoord);
    float t = fract(shift*8);
    presentColor(vec4(a*(1-t)+b*t));
  }
  if(mode==3)presentColor(getInplaceMeanColor());
  if(mode==4)presentColor(getInplaceVarianceColor());
  if(mode==5)presentColor(getVarianceColor(offset,shift) / (getInplaceVarianceColor()));
  if(mode==6)presentColor(getDepth());
  if(mode==7)presentColor(getDepth2());


}
