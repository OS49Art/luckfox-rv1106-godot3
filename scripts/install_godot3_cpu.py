"""Install an opt-in CPU rasterizer into the explicitly specified 3.6.3 checkout."""
from pathlib import Path
import shutil
import sys
import runpy

sys.dont_write_bytecode = True
from check_source import check
root=check(sys.argv[1])
version=runpy.run_path(str(root/'version.py'))
if tuple(version[k] for k in ('major','minor','patch')) != (3,6,3): raise SystemExit('Requires 3.6.3')
kit=Path(__file__).resolve().parent.parent
path=root/'platform/server/os_server.cpp'
text=path.read_text()
# Upgrade the earlier prototype, which flushed events but polled touch only
# during rendering. Event polling must continue when static rendering sleeps.
old_poll='\t\tif (getenv("GODOT_CPU_RENDER")) input->flush_buffered_events();'
new_poll='\t\tif (getenv("GODOT_CPU_RENDER")) { RasterizerCpu::pump_events(); input->flush_buffered_events(); }'
if old_poll in text:text=text.replace(old_poll,new_poll)
edits=[
 ('#include "os_server.h"','#include "os_server.h"\n#include "drivers/dummy/rasterizer_cpu.h"'),
 ('\tRasterizerDummy::make_current();','\tif (getenv("GODOT_CPU_RENDER")) RasterizerCpu::make_current();\n\telse RasterizerDummy::make_current();'),
 ('\tResourceLoader::add_resource_format_loader(resource_loader_dummy);','\tif (!getenv("GODOT_CPU_RENDER")) ResourceLoader::add_resource_format_loader(resource_loader_dummy);'),
 ('\tResourceLoader::remove_resource_format_loader(resource_loader_dummy);','\tif (!getenv("GODOT_CPU_RENDER")) ResourceLoader::remove_resource_format_loader(resource_loader_dummy);'),
 ('\treturn false; //can never draw','\treturn getenv("GODOT_CPU_RENDER") != nullptr; // opt-in CPU Canvas prototype'),
 ('\twhile (!force_quit) {','\twhile (!force_quit) {\n'+new_poll),
 ('Point2 OS_Server::get_mouse_position() const {\n\treturn Point2();','Point2 OS_Server::get_mouse_position() const {\n\tif (getenv("GODOT_CPU_RENDER") && input) return input->get_mouse_position();\n\treturn Point2();'),
]
for before,after in edits:
 if after not in text:
  if text.count(before)!=1: raise SystemExit('Unexpected upstream text; refusing patch')
  text=text.replace(before,after)
path.write_text(text)
for name in ('rasterizer_cpu.h','rasterizer_cpu.cpp','cpu_viewport_mapping.h','cpu_required_device.h'):
 shutil.copyfile(kit/'port/godot3'/name,root/'drivers/dummy'/name)
print('Installed opt-in CPU Canvas subset, without changing the SDK or board.')
