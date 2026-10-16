// CPU-only Canvas2D subset; see docs/ARCHITECTURE.md.
#include "rasterizer_cpu.h"
#include "cpu_required_device.h"
#include "core/os/os.h"
#include "core/os/input.h"
#include "core/os/input_event.h"
#include "core/project_settings.h"
#include "core/io/resource_loader.h"
#include "scene/resources/texture.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <dlfcn.h>
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#if defined(__GNUC__)
#define CPU_HOT __attribute__((optimize("O3")))
#else
#define CPU_HOT
#endif
RasterizerCpu *RasterizerCpu::active_cpu=nullptr;

static uint8_t channel(float v) { return uint8_t(CLAMP(int(v*255+0.5f),0,255)); }
static uint32_t packed(const Color& c) { return 0xff000000u | uint32_t(channel(c.r))<<16 | uint32_t(channel(c.g))<<8 | channel(c.b); }
static CPU_HOT bool same_row(const uint32_t *a,const uint32_t *b,int count) {
#ifdef __ARM_NEON
 uint32x4_t diff=vdupq_n_u32(0);int x=0;
 for(;x+4<=count;x+=4)diff=vorrq_u32(diff,veorq_u32(vld1q_u32(a+x),vld1q_u32(b+x)));
 if(vgetq_lane_u64(vreinterpretq_u64_u32(diff),0)|vgetq_lane_u64(vreinterpretq_u64_u32(diff),1))return false;
 for(;x<count;x++)if(a[x]!=b[x])return false;
 return true;
#else
 return memcmp(a,b,size_t(count)*4)==0;
#endif
}

void CpuStorage::render_target_set_size(RID r,int w,int h) {
 Target *t=targets.getornull(r); ERR_FAIL_COND(!t);
 ERR_FAIL_COND_MSG(w<0||h<0||w>1024||h>1024,"CPU prototype supports targets up to 1024x1024 only");
 t->w=w; t->h=h; t->pixels.assign(size_t(w)*h,0xff000000);
}
bool CpuStorage::free(RID r) {
 if(targets.owns(r)) { Target *t=targets.getornull(r); if(current==t)current=nullptr; targets.free(r); memdelete(t); return true; }
 converted.erase(r); return RasterizerStorageDummy::free(r);
}
Ref<Image> CpuStorage::rgba(RID r) {
 if(converted.has(r))return converted[r];
 DummyTexture *t=texture_owner.getornull(r);
 if(!t||t->image.is_null()||t->image->empty())return Ref<Image>();
 Ref<Image> image=t->image->duplicate(); image->convert(Image::FORMAT_RGBA8);
 converted[r]=image; return image;
}

CPU_HOT void CpuCanvas::rect(const Rect2& area,RID tex,const Rect2& src,uint8_t flags,const Color& color,const Transform2D& xf,const Rect2& clip) {
 auto target=storage->current;
 if(!target||target->pixels.empty()||area.size.x<=0||area.size.y<=0)return;
 if(Math::abs(xf.determinant())<0.00001f)return;
 Ref<Image> image=tex.is_valid()?storage->rgba(tex):Ref<Image>();
 if(tex.is_valid()&&image.is_null()) { WARN_PRINT_ONCE("CPU: missing texture/proxy/render-target texture unsupported"); return; }
 Rect2 bounds=xf.xform(area).clip(clip).clip(Rect2(0,0,target->w,target->h));
 int x0=MAX(0,int(std::floor(bounds.position.x))),y0=MAX(0,int(std::floor(bounds.position.y)));
 int x1=MIN(target->w,int(std::ceil(bounds.position.x+bounds.size.x))),y1=MIN(target->h,int(std::ceil(bounds.position.y+bounds.size.y)));
 Transform2D inv=xf.affine_inverse();
 PoolVector<uint8_t> data; PoolVector<uint8_t>::Read pixels;
 Rect2 source=src; int tw=0,th=0;
 if(image.is_valid()) { tw=image->get_width(); th=image->get_height(); data=image->get_data(); pixels=data.read(); if(!(flags&CANVAS_RECT_REGION))source=Rect2(0,0,tw,th); }
 // Pixel-art fast path: integer translation and 1:1 sampling. Keep the generic
 // inverse-affine path below for rotations/scaling; do not change pixel results.
 Vector2 origin=area.position+xf.get_origin();
 bool integer_origin=origin.x==Math::floor(origin.x)&&origin.y==Math::floor(origin.y);
 bool identity_basis=xf[0]==Vector2(1,0)&&xf[1]==Vector2(0,1);
 bool unit_source=image.is_null()||(source.size==area.size&&source.position.x==Math::floor(source.position.x)&&source.position.y==Math::floor(source.position.y)&&source.position.x>=0&&source.position.y>=0&&source.position.x+source.size.x<=tw&&source.position.y+source.size.y<=th);
 if(integer_origin&&identity_basis&&unit_source&&!(flags&~(CANVAS_RECT_REGION|CANVAS_RECT_CLIP_UV))&&area.size.x==Math::floor(area.size.x)&&area.size.y==Math::floor(area.size.y)) {
  const unsigned tr=channel(color.r),tg=channel(color.g),tb=channel(color.b),ta=channel(color.a);
  const bool white=tr==255&&tg==255&&tb==255;
  for(int y=y0;y<y1;y++) {
   uint32_t *dst=target->pixels.data()+size_t(y)*target->w+x0;
   if(image.is_null()&&ta==255) { std::fill(dst,dst+(x1-x0),packed(color));continue; }
   const uint8_t *src=image.is_valid()?pixels.ptr()+4*(size_t(int(source.position.y)+y-int(origin.y))*tw+int(source.position.x)+x0-int(origin.x)):nullptr;
   for(int x=x0;x<x1;x++,dst++) {
    unsigned r=tr,g=tg,b=tb,a=ta;
    if(src) {
     a=(src[3]*ta+127)/255;
     if(white) {r=src[0];g=src[1];b=src[2];}
     else {r=(src[0]*tr+127)/255;g=(src[1]*tg+127)/255;b=(src[2]*tb+127)/255;}
     src+=4;
    }
    if(a==255)*dst=0xff000000u|r<<16|g<<8|b;
    else if(a) { unsigned inva=255-a;*dst=0xff000000u|((r*a+((*dst>>16)&255)*inva+127)/255)<<16|((g*a+((*dst>>8)&255)*inva+127)/255)<<8|(b*a+(*dst&255)*inva+127)/255; }
   }
  }
  return;
 }
 for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++) {
  Vector2 local=inv.xform(Vector2(x+0.5f,y+0.5f));
  float u=(local.x-area.position.x)/area.size.x,v=(local.y-area.position.y)/area.size.y;
  if(u<0||v<0||u>=1||v>=1)continue;
  Color c=color;
  if(image.is_valid()) {
   if(flags&CANVAS_RECT_TRANSPOSE)std::swap(u,v);
   if(flags&CANVAS_RECT_FLIP_H)u=1-u;
   if(flags&CANVAS_RECT_FLIP_V)v=1-v;
   int sx=int(source.position.x+u*source.size.x),sy=int(source.position.y+v*source.size.y);
   if(flags&CANVAS_RECT_TILE) { sx=((sx%tw)+tw)%tw; sy=((sy%th)+th)%th; }
   else { sx=CLAMP(sx,0,tw-1); sy=CLAMP(sy,0,th-1); }
   const uint8_t *p=pixels.ptr()+4*(size_t(sy)*tw+sx);
   c*=Color(p[0]/255.f,p[1]/255.f,p[2]/255.f,p[3]/255.f);
  }
  unsigned a=channel(c.a); if(!a)continue;
  uint32_t &dst=target->pixels[size_t(y)*target->w+x];
  if(a==255)dst=packed(c);
  else { unsigned b=255-a; dst=0xff000000u | ((channel(c.r)*a+((dst>>16)&255)*b+127)/255)<<16 | ((channel(c.g)*a+((dst>>8)&255)*b+127)/255)<<8 | (channel(c.b)*a+(dst&255)*b+127)/255; }
 }
}

CPU_HOT void CpuCanvas::polygon(const Item::CommandPolygon *p,const Color& tint,const Transform2D& xf,const Rect2& clip) {
 if(p->texture.is_valid()||p->bones.size()) { WARN_PRINT_ONCE("CPU: textured/skinned polygons unsupported");return; }
 if(p->count>p->indices.size()) { ERR_PRINT_ONCE("CPU: invalid polygon index count");return; }
 auto target=storage->current;
 for(int index=0;index+2<p->count;index+=3) {
  int ids[3]={p->indices[index],p->indices[index+1],p->indices[index+2]};
  Vector2 v[3];Color colors[3];
  bool valid=true;
  for(int n=0;n<3;n++) {
   if(ids[n]<0||ids[n]>=p->points.size()) { valid=false;break; }
   v[n]=xf.xform(p->points[ids[n]]);
   colors[n]=p->colors.size()==1?p->colors[0]:(p->colors.size()==p->points.size()?p->colors[ids[n]]:Color(1,1,1,1));
  }
  if(!valid)continue;
  float det=(v[1]-v[0]).cross(v[2]-v[0]); if(Math::abs(det)<0.00001f)continue;
  Rect2 bounds(v[0],Vector2());bounds.expand_to(v[1]);bounds.expand_to(v[2]);bounds=bounds.clip(clip).clip(Rect2(0,0,target->w,target->h));
  int x0=MAX(0,int(floor(bounds.position.x))),y0=MAX(0,int(floor(bounds.position.y)));
  int x1=MIN(target->w,int(ceil(bounds.position.x+bounds.size.x))),y1=MIN(target->h,int(ceil(bounds.position.y+bounds.size.y)));
  for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++) {
   Vector2 d=Vector2(x+0.5f,y+0.5f)-v[0];
   float b=d.cross(v[2]-v[0])/det,c=(v[1]-v[0]).cross(d)/det,a=1-b-c;
   if(a<0||b<0||c<0)continue;
   Color color=(colors[0]*a+colors[1]*b+colors[2]*c)*tint;
   uint32_t &dst=target->pixels[size_t(y)*target->w+x];unsigned alpha=channel(color.a),other=255-alpha;
   dst=0xff000000u|((channel(color.r)*alpha+((dst>>16)&255)*other+127)/255)<<16|((channel(color.g)*alpha+((dst>>8)&255)*other+127)/255)<<8|(channel(color.b)*alpha+(dst&255)*other+127)/255;
  }
 }
}

CPU_HOT void CpuCanvas::canvas_render_items(Item *item,int,const Color& modulate,Light*,const Transform2D&) {
 if(!storage->current)return;
 for(;item;item=item->next) {
  if(item->material.is_valid()||item->material_owner) { WARN_PRINT_ONCE("CPU: shaders/materials unsupported"); }
  Transform2D extra;
  Rect2 clip= item->final_clip_owner?item->final_clip_owner->final_clip_rect:Rect2(0,0,storage->current->w,storage->current->h);
  Color tint=item->final_modulate*modulate;
  for(int i=0;i<item->commands.size();i++) {
   auto command=item->commands[i];
   switch(command->type) {
    case Item::Command::TYPE_TRANSFORM: extra=static_cast<Item::CommandTransform*>(command)->xform; break;
    case Item::Command::TYPE_RECT: {
     auto r=static_cast<Item::CommandRect*>(command);
     rect(r->rect,r->texture,r->source,r->flags,r->modulate*tint,item->final_transform*extra,clip);
    } break;
    case Item::Command::TYPE_MULTIRECT: {
     auto r=static_cast<Item::CommandMultiRect*>(command);
     for(int n=0;n<r->rects.size();n++)rect(r->rects[n],r->texture,n<r->sources.size()?r->sources[n]:Rect2(),r->flags,r->modulate*tint,item->final_transform*extra,clip);
    } break;
    case Item::Command::TYPE_POLYGON: polygon(static_cast<Item::CommandPolygon*>(command),tint,item->final_transform*extra,clip);break;
    default: WARN_PRINT_ONCE("CPU: unsupported canvas command (only RECT/MULTIRECT/TRANSFORM/solid POLYGON accepted)"); break;
   }
  }
 }
}

void RasterizerCpu::open_fb() {
 const char *path=getenv("GODOT_CPU_FB");
 const char *touch=getenv("GODOT_CPU_TOUCH");
 cpu_require_device(!touch||path,"touch requested without framebuffer");
 if(!path)return;
 cpu_require_device(strcmp(path,"/dev/fb0")==0,"framebuffer path must be /dev/fb0");
 fb_fd=open(path,O_RDWR|O_CLOEXEC); cpu_require_device(fb_fd>=0,"cannot open fb0");
 fb_var_screeninfo v={}; fb_fix_screeninfo f={};
 if(ioctl(fb_fd,FBIOGET_VSCREENINFO,&v)||ioctl(fb_fd,FBIOGET_FSCREENINFO,&f)||v.bits_per_pixel!=32||v.red.offset!=16||v.green.offset!=8||v.blue.offset!=0||v.red.length!=8||v.green.length!=8||v.blue.length!=8||v.xres==0||v.yres==0||v.xres>1024||v.yres>1024||f.line_length<4*(v.xoffset+v.xres)||uint64_t(f.line_length)*(v.yoffset+v.yres)>f.smem_len) {
  close(fb_fd);fb_fd=-1; cpu_require_device(false,"unsupported framebuffer format/stride/bounds");
 }
 fb_w=v.xres; fb_h=v.yres; fb_stride=f.line_length; fb_x=v.xoffset; fb_y=v.yoffset; fb_bytes=f.smem_len;
 fb=(uint8_t*)mmap(nullptr,fb_bytes,PROT_READ|PROT_WRITE,MAP_SHARED,fb_fd,0);
 if(fb==MAP_FAILED) { fb=nullptr;close(fb_fd);fb_fd=-1; cpu_require_device(false,"framebuffer mmap failed"); }
 backup.assign(fb,fb+fb_bytes);
 scanline.assign(fb_w,0xff000000u);
 fprintf(stdout,"CPU_FB_READY %dx%d stride=%d\n",fb_w,fb_h,fb_stride);
 cpu_require_device(!touch||strcmp(touch,"/dev/input/event0")==0,"touch path must be /dev/input/event0");
 if(touch) {
  touch_fd=open(touch,O_RDONLY|O_NONBLOCK|O_CLOEXEC);
  char name[128]={};
  if(touch_fd>=0&&(ioctl(touch_fd,EVIOCGNAME(sizeof(name)),name)<0||!strstr(name,"Goodix")||ioctl(touch_fd,EVIOCGABS(ABS_MT_POSITION_X),&touch_range_x)||ioctl(touch_fd,EVIOCGABS(ABS_MT_POSITION_Y),&touch_range_y)||touch_range_x.maximum<=touch_range_x.minimum||touch_range_y.maximum<=touch_range_y.minimum||ioctl(touch_fd,EVIOCGRAB,1))) {
   close(touch_fd);touch_fd=-1;
  }
  cpu_require_device(touch_fd>=0,"touch open/name/axes/grab failed");
  fprintf(stdout,"CPU_TOUCH_READY enabled=1\n");
 }
}
void RasterizerCpu::mouse(int x,int y,bool pressed) {
 Ref<InputEventMouseMotion> motion; motion.instance();motion->set_position(Vector2(x,y));motion->set_global_position(Vector2(x,y));Input::get_singleton()->parse_input_event(motion);
 Ref<InputEventMouseButton> click;click.instance();click->set_button_index(BUTTON_LEFT);click->set_pressed(pressed);click->set_position(Vector2(x,y));click->set_global_position(Vector2(x,y));click->set_button_mask(pressed?BUTTON_MASK_LEFT:0);Input::get_singleton()->parse_input_event(click);
}
void RasterizerCpu::poll_touch() {
 if(touch_fd<0||!screen||screen->w<=0||screen->h<=0)return;
 input_event e;
 while(read(touch_fd,&e,sizeof(e))==sizeof(e)) {
  if(e.type==EV_ABS&&e.code==ABS_MT_SLOT)touch_slot=e.value;
  if(touch_slot==0&&e.type==EV_ABS) {
   if(e.code==ABS_MT_POSITION_X)touch_x=e.value;
   if(e.code==ABS_MT_POSITION_Y)touch_y=e.value;
   if(e.code==ABS_MT_TRACKING_ID)touch_down=e.value>=0;
  }
  if(e.type==EV_KEY&&e.code==BTN_TOUCH)touch_down=e.value!=0;
  if(e.type==EV_SYN&&e.code==SYN_REPORT&&touch_down!=touch_sent) {
   int px=(touch_x-touch_range_x.minimum)*(fb_w-1)/(touch_range_x.maximum-touch_range_x.minimum);
   int py=(touch_y-touch_range_y.minimum)*(fb_h-1)/(touch_range_y.maximum-touch_range_y.minimum);
   int x=-1,y=-1;
   if(!viewport_mapping.to_logical(px,py,x,y)&&touch_down)continue;
   mouse(x,y,touch_down);touch_sent=touch_down;
   fprintf(stdout,"CPU_PHYSICAL_TOUCH x=%d y=%d pressed=%d\n",x,y,touch_down);
  }
 }
}
void RasterizerCpu::initialize() {
 setvbuf(stdout,nullptr,_IOLBF,0);
 const char *mode=getenv("GODOT_CPU_SCALE");fit_height=mode&&strcmp(mode,"height")==0;
 full_present=getenv("GODOT_CPU_PRESENT_FULL")!=nullptr;
 // Public CPU-only distribution: no RGA library or physical-address probes.
 if(getenv("GODOT_CPU_RGA")) WARN_PRINT_ONCE("RGA is not included in this CPU-only distribution");
 fprintf(stdout,"CPU_CANVAS_READY subset=rect,multirect,transform,texture,alpha,clip,solid_polygon\n");open_fb();
}
void RasterizerCpu::begin_frame(double) {
 if(getenv("GODOT_CPU_TEST_CLICK")&&(frames==59||frames==60)) { mouse(160,195,frames==59);fprintf(stdout,"CPU_SYNTHETIC_CLICK pressed=%d\n",frames==59); }
 begin_us=OS::get_singleton()->get_ticks_usec();
}
void RasterizerCpu::set_current_render_target(RID r) { cpu_storage.current=r.is_valid()?cpu_storage.targets.getornull(r):nullptr; }
void RasterizerCpu::clear_render_target(const Color& color) {
 auto t=cpu_storage.current; if(t)std::fill(t->pixels.begin(),t->pixels.end(),packed(color));
}
CPU_HOT void RasterizerCpu::prepare_backdrop() {
 if(backdrop_ready)return;
 backdrop_ready=true;
 auto settings=ProjectSettings::get_singleton();
 if(!settings->has_setting("rv1106_presentation/background_color")&&!settings->has_setting("rv1106_presentation/background_texture"))return;
 Color base(0,0,0,1);
 if(settings->has_setting("rv1106_presentation/background_color"))base=settings->get("rv1106_presentation/background_color");
 const bool diagnostic_solid=getenv("GODOT_CPU_BACKGROUND_SOLID")!=nullptr;
 if(diagnostic_solid)base=Color(0.12f,0.12f,0.12f,1);
 const uint32_t fill=packed(base);
 backdrop.assign(size_t(fb_w)*fb_h,fill);
 String path=settings->has_setting("rv1106_presentation/background_texture")?String(settings->get("rv1106_presentation/background_texture")):String();
 if(diagnostic_solid)path=String();
 Ref<Texture> texture;
 if(path.begins_with("res://"))texture=ResourceLoader::load(path);
 Ref<Image> image;
 if(texture.is_valid())image=texture->get_data();
 if(image.is_valid()&&!image->empty()) {
  image=image->duplicate();image->convert(Image::FORMAT_RGBA8);
  const int w=image->get_width(),h=image->get_height();
  PoolVector<uint8_t> data=image->get_data();auto pixels=data.read();
  const bool width_cover=int64_t(fb_w)*h>=int64_t(fb_h)*w;
  std::vector<int> xs(fb_w),ys(fb_h);
  // Precompute exact cover sampling; no division in the pixel loop.
  for(int x=0;x<fb_w;x++)xs[x]=CLAMP(width_cover?int(int64_t(2*x+1)*w/(2*fb_w)):int((int64_t(w)*fb_h-int64_t(fb_w)*h+int64_t(2*x+1)*h)/(2*fb_h)),0,w-1);
  for(int y=0;y<fb_h;y++)ys[y]=CLAMP(width_cover?int((int64_t(h)*fb_w-int64_t(fb_h)*w+int64_t(2*y+1)*w)/(2*fb_w)):int(int64_t(2*y+1)*h/(2*fb_h)),0,h-1);
  for(int y=0;y<fb_h;y++) {
   if(y&&ys[y]==ys[y-1]){memcpy(backdrop.data()+size_t(y)*fb_w,backdrop.data()+size_t(y-1)*fb_w,size_t(fb_w)*4);continue;}
   for(int x=0;x<fb_w;x++) {
   int sx=xs[x],sy=ys[y];
   const uint8_t *p=pixels.ptr()+4*(size_t(sy)*w+sx);
   unsigned a=p[3],b=255-a;
   if(a==255){backdrop[size_t(y)*fb_w+x]=0xff000000u|uint32_t(p[0])<<16|uint32_t(p[1])<<8|p[2];continue;}
   if(!a)continue;
   backdrop[size_t(y)*fb_w+x]=0xff000000u|
    ((p[0]*a+((fill>>16)&255)*b+127)/255)<<16|
    ((p[1]*a+((fill>>8)&255)*b+127)/255)<<8|
    (p[2]*a+(fill&255)*b+127)/255;
   }
  }
  fprintf(stdout,"CPU_BACKDROP mode=cover filter=nearest source=%dx%d output=%dx%d cached=true\n",w,h,fb_w,fb_h);
 } else fprintf(stdout,"CPU_BACKDROP mode=color output=%dx%d\n",fb_w,fb_h);
 // Experimental module currently has black margins only; preserve exact backdrop.
 if(rga_presenter){rga_close(rga_presenter);rga_presenter=nullptr;fprintf(stdout,"CPU_RGA_DISABLED reason=full-screen-backdrop\n");}
}
CPU_HOT void RasterizerCpu::blit_render_target_to_screen(RID r,const Rect2&,int) {
 screen=cpu_storage.targets.getornull(r); if(!screen||!fb||screen->w<=0||screen->h<=0)return;
 CpuViewportMapping next=CpuViewportMapping::make(screen->w,screen->h,fb_w,fb_h,fit_height);
 if(!next.valid) { WARN_PRINT_ONCE("CPU: selected viewport scale does not fit framebuffer");return; }
 uint64_t present_start=OS::get_singleton()->get_ticks_usec();
 prepare_backdrop();
 bool reset=!viewport_mapping.valid||viewport_mapping.width!=next.width||viewport_mapping.height!=next.height||viewport_mapping.source_w!=next.source_w||viewport_mapping.source_h!=next.source_h||viewport_mapping.left!=next.left||viewport_mapping.top!=next.top;
 if(!viewport_mapping.valid||viewport_mapping.width!=next.width||viewport_mapping.height!=next.height||viewport_mapping.source_w!=next.source_w||viewport_mapping.source_h!=next.source_h) {
  fprintf(stdout,"CPU_VIEWPORT mode=%s source=%dx%d output=%dx%d left=%d top=%d filter=nearest\n",fit_height?"height":"integer",screen->w,screen->h,next.width,next.height,next.left,next.top);
  fflush(stdout);
 }
 viewport_mapping=next;
 if(int(screen_x_lut.size())!=next.width||cached_source_width!=screen->w) {
  screen_x_lut.resize(next.width);cached_source_width=screen->w;
  for(int x=0;x<next.width;x++)screen_x_lut[x]=next.source_x(x);
 }
 if(rga_presenter) {
  if(rga_submit(rga_presenter,screen->pixels.data(),screen->w,screen->h,screen_x_lut.data(),next.width,next.height,next.left,next.top)==0) {
   present_us+=OS::get_singleton()->get_ticks_usec()-present_start;
   present_bytes+=size_t(fb_w)*fb_h*4;return;
  }
  rga_close(rga_presenter);rga_presenter=nullptr;
  previous_pixels.clear();reset=true;
  WARN_PRINT_ONCE("RGA submission failed: falling back to CPU presenter");
 }
 if(!full_present) {
  if(reset||previous_pixels.size()!=screen->pixels.size()) {
   previous_pixels.resize(screen->pixels.size());reset=true;
   std::fill(scanline.begin(),scanline.end(),0xff000000u);
   for(int y=0;y<fb_h;y++)memcpy(fb+(y+fb_y)*fb_stride+fb_x*4,backdrop.empty()?scanline.data():backdrop.data()+size_t(y)*fb_w,fb_w*4);
   present_bytes+=size_t(fb_w)*fb_h*4;
  }
  int last_y=-1,dx0=0,dx1=0;
  for(int dy=0;dy<next.height;dy++) {
   int sy=next.source_y(dy);
   if(sy!=last_y) {
    const uint32_t *src=screen->pixels.data()+size_t(sy)*screen->w;
    uint32_t *old=previous_pixels.data()+size_t(sy)*screen->w;
    int x0=0,x1=screen->w;
    if(!reset&&same_row(src,old,screen->w))x0=x1;
    else if(!reset) {
     while(x0<x1&&src[x0]==old[x0])x0++;
     while(x1>x0&&src[x1-1]==old[x1-1])x1--;
    }
    dx0=int(std::lower_bound(screen_x_lut.begin(),screen_x_lut.end(),x0)-screen_x_lut.begin());
    dx1=int(std::lower_bound(screen_x_lut.begin(),screen_x_lut.end(),x1)-screen_x_lut.begin());
    for(int x=dx0;x<dx1;x++)scanline[next.left+x]=src[screen_x_lut[x]];
    if(x1>x0)memcpy(old+x0,src+x0,size_t(x1-x0)*4);
    last_y=sy;
   }
   if(dx1>dx0) {
    size_t bytes=size_t(dx1-dx0)*4;
    memcpy(fb+(next.top+dy+fb_y)*fb_stride+(fb_x+next.left+dx0)*4,scanline.data()+next.left+dx0,bytes);
    present_bytes+=bytes;
   }
  }
  present_us+=OS::get_singleton()->get_ticks_usec()-present_start;
  return;
 }
 // Expand a row once, reuse it for repeated nearest-neighbour source rows.
 // Pixel sampling and input use the same mapping, including non-integer ratios.
 std::fill(scanline.begin(),scanline.end(),0xff000000u);
 if(backdrop.empty()) {
  for(int y=0;y<next.top;y++)memcpy(fb+(y+fb_y)*fb_stride+fb_x*4,scanline.data(),fb_w*4);
  for(int y=next.top+next.height;y<fb_h;y++)memcpy(fb+(y+fb_y)*fb_stride+fb_x*4,scanline.data(),fb_w*4);
 } else if(reset) {
  for(int y=0;y<fb_h;y++)memcpy(fb+(y+fb_y)*fb_stride+fb_x*4,backdrop.data()+size_t(y)*fb_w,fb_w*4);
  present_bytes+=size_t(fb_w)*fb_h*4;
 }
 int last_source_y=-1;
 for(int dy=0;dy<next.height;dy++) {
  int sy=next.source_y(dy);
  if(sy!=last_source_y) {
   const uint32_t *src=screen->pixels.data()+size_t(sy)*screen->w;
   for(int dx=0;dx<next.width;dx++)scanline[next.left+dx]=src[screen_x_lut[dx]];
   last_source_y=sy;
  }
  uint8_t *dst=fb+(next.top+dy+fb_y)*fb_stride+fb_x*4;
  if(backdrop.empty())memcpy(dst,scanline.data(),fb_w*4);
  else memcpy(dst+next.left*4,scanline.data()+next.left,size_t(next.width)*4);
 }
 present_bytes+=backdrop.empty()?size_t(fb_w)*fb_h*4:size_t(next.width)*next.height*4;
 present_us+=OS::get_singleton()->get_ticks_usec()-present_start;
}
void RasterizerCpu::dump(const char *path) {
 if(!screen||screen->pixels.empty())return;
 FILE *file=fopen(path,"wb"); if(!file) { WARN_PRINT_ONCE("CPU: failed to open diagnostic PPM");return; }
 fprintf(file,"P6\n%d %d\n255\n",screen->w,screen->h);
 for(auto p:screen->pixels) { unsigned char rgb[3]={uint8_t(p>>16),uint8_t(p>>8),uint8_t(p)}; if(fwrite(rgb,1,3,file)!=3)break; }
 fclose(file); fprintf(stdout,"CPU_FRAME_DUMP frame=%u size=%dx%d\n",frames,screen->w,screen->h);
}
void RasterizerCpu::end_frame(bool) {
 frames++; draw_us+=OS::get_singleton()->get_ticks_usec()-begin_us;
 if(getenv("GODOT_CPU_VERIFY_FB")&&fb&&screen&&viewport_mapping.valid&&(frames==1||frames==30||frames==90||frames==120||frames==180)) {
  uint64_t different=0;
  for(int y=0;y<fb_h;y++)for(int x=0;x<fb_w;x++) {
   int sx,sy;uint32_t expected=backdrop.empty()?0xff000000u:backdrop[size_t(y)*fb_w+x];
   if(viewport_mapping.to_logical(x,y,sx,sy))expected=screen->pixels[size_t(sy)*screen->w+sx];
   uint32_t actual;memcpy(&actual,fb+(y+fb_y)*fb_stride+(x+fb_x)*4,4);
   if(actual!=expected)different++;
  }
  fprintf(stdout,"CPU_VERIFY_FB frame=%u different_pixels=%llu\n",frames,(unsigned long long)different);
 }
 const char *path=getenv("GODOT_CPU_DUMP");
 if(path&&(frames==30||frames==90||(frames==1&&getenv("GODOT_CPU_DUMP_FIRST")))) { char name[1024];snprintf(name,sizeof(name),"%s-%u.ppm",path,frames);dump(name); }
 if(frames==120||frames%900==0)fprintf(stdout,"CPU_RENDER_STATS frames=%u mean_render_us=%llu\n",frames,(unsigned long long)(draw_us/frames));
}
void RasterizerCpu::finalize() {
 active_cpu=nullptr;
 bool used_rga=rga_presenter!=nullptr;
 if(rga_presenter){rga_close(rga_presenter);rga_presenter=nullptr;}
 if(rga_library){dlclose(rga_library);rga_library=nullptr;}
 if(touch_fd>=0) {ioctl(touch_fd,EVIOCGRAB,0);close(touch_fd);touch_fd=-1;}
 if(fb) { memcpy(fb,backup.data(),fb_bytes);munmap(fb,fb_bytes);fb=nullptr; }
 if(fb_fd>=0) {close(fb_fd);fb_fd=-1;}
 if(frames)fprintf(stdout,"CPU_RENDER_STATS frames=%u mean_render_us=%llu\n",frames,(unsigned long long)(draw_us/frames));
 if(frames)fprintf(stdout,"CPU_PRESENT_STATS mode=%s mean_us=%llu mean_bytes=%llu\n",used_rga?"rga-copy":(full_present?"full":"dirty"),(unsigned long long)(present_us/frames),(unsigned long long)(present_bytes/frames));
 fprintf(stdout,"CPU_CANVAS_FINISH frames=%u\n",frames);
}
