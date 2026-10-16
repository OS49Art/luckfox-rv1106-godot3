// Shared framebuffer presentation and touch inverse mapping, no Godot deps.
#ifndef CPU_VIEWPORT_MAPPING_H
#define CPU_VIEWPORT_MAPPING_H
struct CpuViewportMapping {
 int source_w=0,source_h=0,width=0,height=0,left=0,top=0;
 bool valid=false;
 static CpuViewportMapping make(int sw,int sh,int fw,int fh,bool fit_height) {
  CpuViewportMapping m;
  if(sw<=0||sh<=0||fw<=0||fh<=0||sw>4096||sh>4096||fw>4096||fh>4096)return m;
  m.source_w=sw;m.source_h=sh;
  if(fit_height) { m.height=fh;m.width=(sw*fh+sh/2)/sh; }
  else { int scale=fw/sw<fh/sh?fw/sw:fh/sh;m.width=sw*scale;m.height=sh*scale; }
  if(m.width<=0||m.height<=0||m.width>fw||m.height>fh)return m;
  m.left=(fw-m.width)/2;m.top=(fh-m.height)/2;m.valid=true;return m;
 }
 // Nearest neighbour at output pixel centers, also used for touch conversion.
 int source_x(int dx) const { return ((2*dx+1)*source_w)/(2*width); }
 int source_y(int dy) const { return ((2*dy+1)*source_h)/(2*height); }
 bool to_logical(int px,int py,int &x,int &y) const {
  x=y=-1;
  if(!valid||px<left||py<top||px>=left+width||py>=top+height)return false;
  x=source_x(px-left);y=source_y(py-top);return true;
 }
};
#endif
