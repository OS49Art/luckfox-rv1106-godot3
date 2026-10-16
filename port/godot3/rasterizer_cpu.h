// Godot 3.6.3 CPU Canvas subset. Optional RGA final-copy experiment, no GL/3D/shaders.
#ifndef GODOT3_RASTERIZER_CPU_H
#define GODOT3_RASTERIZER_CPU_H
#include "drivers/dummy/rasterizer_dummy.h"
#include "core/map.h"
#include "cpu_viewport_mapping.h"
#include <vector>
#include <linux/input.h>

class CpuStorage : public RasterizerStorageDummy {
public:
 struct Target : RID_Data { int w=0, h=0; std::vector<uint32_t> pixels; };
 RID_Owner<Target> targets;
 Map<RID, Ref<Image>> converted;
 Target *current=nullptr;
 RID render_target_create() override { return targets.make_rid(memnew(Target)); }
 void render_target_set_size(RID r, int w, int h) override;
 bool free(RID r) override;
 Ref<Image> rgba(RID r);
 void texture_set_data(RID r, const Ref<Image>& image, int level) override {
  converted.erase(r); RasterizerStorageDummy::texture_set_data(r,image,level);
 }
 void texture_set_data_partial(RID r,const Ref<Image>& image,int sx,int sy,int sw,int sh,int dx,int dy,int mip,int level) override {
  converted.erase(r); RasterizerStorageDummy::texture_set_data_partial(r,image,sx,sy,sw,sh,dx,dy,mip,level);
 }
 uint32_t texture_get_width(RID r) const override { auto t=texture_owner.getornull(r); return t?t->width:0; }
 uint32_t texture_get_height(RID r) const override { auto t=texture_owner.getornull(r); return t?t->height:0; }
 Size2 texture_size_with_proxy(RID r) const override { return Size2(texture_get_width(r),texture_get_height(r)); }
};

class CpuCanvas : public RasterizerCanvasDummy {
 CpuStorage *storage;
 void rect(const Rect2&,RID,const Rect2&,uint8_t,const Color&,const Transform2D&,const Rect2&);
 void polygon(const Item::CommandPolygon*,const Color&,const Transform2D&,const Rect2&);
public:
 explicit CpuCanvas(CpuStorage *s):storage(s) {}
 void canvas_render_items(Item*,int,const Color&,Light*,const Transform2D&) override;
};

class RasterizerCpu : public RasterizerDummy {
 static RasterizerCpu *active_cpu;
 CpuStorage cpu_storage;
 CpuCanvas cpu_canvas;
 CpuStorage::Target *screen=nullptr;
 int fb_fd=-1;
 int touch_fd=-1,touch_x=0,touch_y=0,touch_slot=0;
 bool touch_down=false,touch_sent=false;
 input_absinfo touch_range_x={},touch_range_y={};
 uint8_t *fb=nullptr;
 size_t fb_bytes=0;
 int fb_w=0,fb_h=0,fb_stride=0,fb_x=0,fb_y=0;
 std::vector<uint8_t> backup;
 std::vector<uint32_t> scanline;
 std::vector<uint32_t> backdrop;
 bool backdrop_ready=false;
 void prepare_backdrop();
 CpuViewportMapping viewport_mapping;
 std::vector<int> screen_x_lut;
 int cached_source_width=0;
 bool fit_height=false;
 bool full_present=false;
 std::vector<uint32_t> previous_pixels;
 uint64_t present_us=0,present_bytes=0;
 void *rga_library=nullptr,*rga_presenter=nullptr;
 int (*rga_submit)(void*,const uint32_t*,int,int,const int*,int,int,int,int)=nullptr;
 void (*rga_close)(void*)=nullptr;
 unsigned frames=0;
 uint64_t begin_us=0,draw_us=0;
 void dump(const char*);
 void open_fb();
 void poll_touch();
 void mouse(int,int,bool);
public:
 RasterizerCpu():cpu_canvas(&cpu_storage) { active_cpu=this; }
 static void pump_events() { if(active_cpu)active_cpu->poll_touch(); }
 RasterizerStorage *get_storage() override { return &cpu_storage; }
 RasterizerCanvas *get_canvas() override { return &cpu_canvas; }
 void initialize() override;
 void begin_frame(double) override;
 void set_current_render_target(RID) override;
 void clear_render_target(const Color&) override;
 void blit_render_target_to_screen(RID,const Rect2&,int) override;
 void end_frame(bool) override;
 void finalize() override;
 static Rasterizer *create_cpu() { return memnew(RasterizerCpu); }
 static void make_current() { _create_func=create_cpu; }
};
#endif
