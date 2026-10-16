#include "cpu_viewport_mapping.h"
#include <cassert>
#include <cstdio>
int main() {
 auto square=CpuViewportMapping::make(360,360,720,720,true);
 assert(square.valid&&square.width==720&&square.height==720&&square.left==0&&square.top==0);
 int sx,sy;
 for(int px=0;px<720;px++)for(int py=0;py<720;py++) {
  assert(square.to_logical(px,py,sx,sy)&&sx==px/2&&sy==py/2);
 }
 assert(!square.to_logical(-1,0,sx,sy));
 assert(!square.to_logical(720,719,sx,sy));
 auto m=CpuViewportMapping::make(270,480,720,720,true);
 assert(m.valid&&m.width==405&&m.height==720&&m.left==157&&m.top==0);
 int x,y;
 assert(m.to_logical(157,0,x,y)&&x==0&&y==0);
 assert(m.to_logical(561,719,x,y)&&x==269&&y==479);
 assert(!m.to_logical(156,200,x,y)&&x==-1&&y==-1);
 assert(!m.to_logical(562,200,x,y));
 bool seen_x[270]={},seen_y[480]={};
 for(int px=157;px<562;px++)for(int py=0;py<720;py++) {
  assert(m.to_logical(px,py,x,y));
  assert(x>=0&&x<270&&y>=0&&y<480);
  assert(x==((2*(px-157)+1)*270)/(2*405));
  assert(y==((2*py+1)*480)/(2*720));
  seen_x[x]=true;seen_y[y]=true;
 }
 for(bool value:seen_x)assert(value);
 for(bool value:seen_y)assert(value);
 // Existing integer mode stays pixel-exact; no 1.5x change to other demos.
 auto old=CpuViewportMapping::make(320,240,720,720,false);
 assert(old.valid&&old.width==640&&old.height==480&&old.left==40&&old.top==120);
 for(int dx=0;dx<640;dx++)assert(old.source_x(dx)==dx/2);
 assert(!CpuViewportMapping::make(0,480,720,720,true).valid);
 assert(!CpuViewportMapping::make(720,240,720,720,true).valid);
 assert(!CpuViewportMapping::make(720,960,720,720,false).valid);
 printf("PASS: 518400 square 2x mappings plus 291600 scaled-pixel/touch mappings, full source coverage, bars, integer regression\n");
}
