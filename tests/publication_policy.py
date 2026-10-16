"""Explicit source allowlist; no parent/sensitive-project traversal."""
import re
ALLOWED = set('''
.gitattributes .gitignore .github/workflows/host-tests.yml LICENSE LICENSES/Godot-MIT.txt
README.md README.zh-CN.md SOURCE_MANIFEST.sha256
demo/export_presets.cfg demo/main.gd demo/main.tscn demo/project.godot
docs/FIGURES.md docs/images/architecture.svg docs/images/architecture.png
docs/images/deployment.svg docs/images/deployment.png
docs/ARCHITECTURE.md docs/DEVICE.md docs/EVIDENCE.md docs/PROVENANCE.md docs/VALIDATION.md
port/godot3/cpu_viewport_mapping.h port/godot3/cpu_required_device.h
port/godot3/rasterizer_cpu.cpp port/godot3/rasterizer_cpu.h port/godot3/test_viewport_mapping.cpp
port/lvgl_entry_adapter.h port/desktop_lifecycle.h port/desktop_supervisor.cpp
scripts/build_godot3.sh scripts/build_supervisor.sh scripts/check_source.py
scripts/install_godot3_cpu.py scripts/launch_desktop.sh scripts/patch_godot3_cross.py scripts/stage_device.sh
tests/demo_smoke.gd tests/test_install.py tests/test_repository.py tests/publication_policy.py
tests/test_lifecycle.cpp tests/test_posix.cpp
'''.split())
PRIVATE_IP = re.compile(r'(?<![0-9])(?:192[.]168[.][0-9]+[.][0-9]+|10[.][0-9]+[.][0-9]+[.][0-9]+|172[.](?:1[6-9]|2[0-9]|3[01])[.][0-9]+[.][0-9]+)(?![0-9])')
FORBIDDEN_CONTENT = tuple(s.lower() for s in ('TX'+'WQ','YH'+'WQ','NC'+'NN'))
MODEL_EXTENSIONS = {'.pt','.pth','.onnx','.rknn','.tflite','.safetensors','.ckpt','.bin','.h5','.pb','.pkl','.npz'}
FORBIDDEN_TREES = {'models','weights','datasets','training','checkpoints','trained_models','training_configs'}
def content_ok(text):
    return not PRIVATE_IP.search(text) and not any(word in text.lower() for word in FORBIDDEN_CONTENT)
def filename_ok(name):
    from pathlib import PurePosixPath
    path = PurePosixPath(name)
    return (name in ALLOWED and path.suffix.lower() not in MODEL_EXTENSIONS and
            not any(p.lower() in FORBIDDEN_TREES for p in path.parts))

PNG_PATHS = {'docs/images/architecture.png','docs/images/deployment.png'}
SVG_PATHS = {'docs/images/architecture.svg','docs/images/deployment.svg'}
PNG_SIGNATURE = bytes((137,80,78,71,13,10,26,10))
def audit_png(data):
    """Only original approved raster diagram exports: no text/EXIF/unknown chunks."""
    import struct
    import zlib
    if not data.startswith(PNG_SIGNATURE):raise ValueError('Not PNG')
    offset=8
    seen=[]
    compressed=[]
    dimensions=None
    allowed={b'IHDR',b'IDAT',b'IEND',b'sRGB',b'gAMA',b'cHRM',b'pHYs'}
    while offset<len(data):
        if offset+12>len(data):raise ValueError('Truncated PNG')
        length=struct.unpack('>I',data[offset:offset+4])[0]
        kind=data[offset+4:offset+8]
        end=offset+12+length
        if length>64*1024*1024 or end>len(data) or kind not in allowed:raise ValueError('Unapproved PNG metadata/chunk')
        payload=data[offset+8:offset+8+length]
        crc=struct.unpack('>I',data[offset+8+length:end])[0]
        if zlib.crc32(kind+payload)&0xffffffff!=crc:raise ValueError('PNG CRC')
        if not seen and kind!=b'IHDR':raise ValueError('IHDR must be first')
        if kind==b'IHDR':
            if dimensions or length!=13:raise ValueError('Bad IHDR')
            width,height,depth,color,compression,filtering,interlace=struct.unpack('>IIBBBBB',payload)
            if not(0<width<=8192 and 0<height<=8192 and width*height<=32000000):raise ValueError('Image dimensions')
            if depth!=8 or color not in (2,6) or compression or filtering or interlace:raise ValueError('Only noninterlaced RGB/RGBA8')
            dimensions=(width,height,3 if color==2 else 4)
        elif kind==b'IDAT':
            if b'IEND' in seen:raise ValueError('IDAT after end')
            compressed.append(payload)
        elif kind==b'IEND':
            if length or end!=len(data) or not compressed:raise ValueError('Bad IEND/trailing bytes')
        else:
            expected={b'sRGB':1,b'gAMA':4,b'cHRM':32,b'pHYs':9}[kind]
            if kind in seen or length!=expected or b'IDAT' in seen:raise ValueError('Invalid ancillary metadata')
        seen.append(kind)
        offset=end
    if not dimensions or not seen or seen[-1]!=b'IEND':raise ValueError('Incomplete PNG')
    width,height,channels=dimensions
    expected=height*(1+width*channels)
    decoder=zlib.decompressobj()
    raw=decoder.decompress(b''.join(compressed),expected+1)
    if len(raw)!=expected or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:raise ValueError('PNG pixel stream size')
    stride=1+width*channels
    if any(raw[y*stride]>4 for y in range(height)):raise ValueError('PNG scanline filter')
    return width,height

def audit_svg(data):
    """Static original vector only: no scripts/images/external references/DTD."""
    import xml.etree.ElementTree as ET
    text=data.decode('utf-8')
    lower=text.lower()
    if not content_ok(text) or any(word in lower for word in ('<!doctype','<!entity','javascript:','@import','@font-face','<?xml-stylesheet')):raise ValueError('SVG active/private content')
    if re.search(r'<[?](?!xml\s)',text,re.I):raise ValueError('SVG processing instruction')
    root=ET.fromstring(text)
    if root.tag!='{http://www.w3.org/2000/svg}svg':raise ValueError('SVG root namespace')
    allowed={'svg','g','metadata','defs','marker','rect','path','line','polyline','polygon','circle','ellipse','text','tspan','title','desc','linearGradient','radialGradient','stop','clipPath','mask','style'}
    for element in root.iter():
        if not element.tag.startswith('{http://www.w3.org/2000/svg}') or element.tag.split('}')[-1] not in allowed:raise ValueError('Unapproved SVG element')
        for attr,value in element.attrib.items():
            key=attr.split('}')[-1].lower()
            if key.startswith('on') or key in ('src','base'):raise ValueError('Active SVG attribute')
            if key=='href' and not value.startswith('#'):raise ValueError('External SVG href')
        for value in list(element.attrib.values())+[element.text or '']:
            for target in re.findall(r'url\s*\(([^)]*)\)',value,re.I):
                if not target.strip().strip(chr(34)+chr(39)).startswith('#'):raise ValueError('External SVG URL')
    return True

def mode_ok(mode):
    """Only regular files; no symbolic links or gitlink/submodule entries."""
    return mode in {'100644','100755'}
