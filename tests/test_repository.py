"""Offline production/window/privacy constraints, not hardware acceptance."""
from pathlib import Path
import ast
import re
import subprocess
import unittest
from publication_policy import ALLOWED, PRIVATE_IP, content_ok, filename_ok, PNG_PATHS, SVG_PATHS, PNG_SIGNATURE, audit_png, audit_svg, mode_ok, SCREENSHOT_DIMENSIONS
ROOT = Path(__file__).resolve().parents[1]
class Repository(unittest.TestCase):
    def test_window(self):
        text = (ROOT/'demo/project.godot').read_text(encoding='utf-8')
        for setting in ('window/size/width=360','window/size/height=360'):self.assertIn(setting,text)
        for setting in ('test_width','test_height'):self.assertNotIn(setting,text)
    def test_syntax(self):
        for path in list((ROOT/'scripts').glob('*.py'))+list((ROOT/'tests').glob('*.py')):
            ast.parse(path.read_text(encoding='utf-8'), filename=path.name)
    def test_cpu_baseline(self):
        text = (ROOT/'scripts/launch_desktop.sh').read_text(encoding='utf-8')
        self.assertIn('GODOT_CPU_PRESENT_FULL=1',text)
        self.assertIn('unset GODOT_CPU_RGA',text)
        self.assertIn('exec ./desktop-supervisor',text)
        cpp = (ROOT/'port/godot3/rasterizer_cpu.cpp').read_text(encoding='utf-8')
        self.assertNotIn('dlopen(',cpp)
        self.assertIn('optimize("O3")',cpp)
        self.assertIn('cpu_require_device(touch_fd>=0',cpp)
        self.assertIn('cpu_require_device(fb_fd>=0',cpp)
    def inspect_text(self, data, name):
        self.assertNotIn(bytes([0]),data,name)
        text=data.decode('utf-8')
        self.assertTrue(content_ok(text),name)
        self.assertIsNone(re.search(r'[A-Z]:[/\\](?:Users|OS2|SOFT)',text),name)
        self.assertIsNone(re.search(r'(?:gh[pousr]_[A-Za-z0-9]{20,}|github_pat_[A-Za-z0-9_]{20,}|-----BEGIN [A-Z ]*PRIVATE KEY-----)',text),name)
        self.assertTrue(all(ord(c)>=32 or c in '\n\r\t' for c in text),name)
    def inspect_artifact(self,data,name):
        if name in PNG_PATHS:
            audit_png(data,name)
        else:
            self.inspect_text(data,name)
            if name in SVG_PATHS:audit_svg(data)
    def test_allowlisted_worktree(self):
        for path in ROOT.rglob('*'):
            if '.git' in path.parts or '__pycache__' in path.parts:continue
            self.assertFalse(path.is_symlink(),str(path))
            if not path.is_file():continue
            name=path.relative_to(ROOT).as_posix()
            self.assertTrue(filename_ok(name),name)
            self.inspect_artifact(path.read_bytes(),name)
    def test_tracked_index_and_history(self):
        # Only this standalone repository; no parent history/project traversal.
        def git(*args):return subprocess.check_output(['git','-C',str(ROOT)]+list(args))
        for row in git('ls-files','--stage').decode().splitlines():
            metadata,name=row.split('\t',1)
            mode,oid,stage=metadata.split()
            self.assertTrue(mode_ok(mode),name+':'+mode)
            self.assertEqual(stage,'0',name)
        for commit in git('rev-list','--all').decode().splitlines():
            for row in git('ls-tree','-r',commit).decode().splitlines():
                metadata,name=row.split('\t',1)
                mode,kind,oid=metadata.split()
                self.assertTrue(mode_ok(mode),name+':'+mode)
                self.assertTrue(filename_ok(name),name)
        png_objects=set()
        for name in git('ls-files').decode().splitlines():
            self.assertTrue(filename_ok(name),name)
            self.inspect_artifact(git('show',':'+name),name)
            if name in PNG_PATHS:png_objects.add(git('rev-parse',':'+name).decode().strip())
        for item in git('rev-list','--objects','--all').decode().splitlines():
            oid, _, name=item.partition(' ')
            if git('cat-file','-t',oid).strip()==b'blob':
                self.assertTrue(filename_ok(name),name)
                self.inspect_artifact(git('cat-file','blob',oid),name)
                if name in PNG_PATHS:png_objects.add(oid)
        # Also scan unreachable/index-replaced blobs; refs alone are insufficient.
        objects=git('cat-file','--batch-all-objects','--batch-check=%(objectname) %(objecttype)').decode().splitlines()
        for row in objects:
            oid, kind=row.split()
            if kind=='tree':
                for entry in git('ls-tree',oid).decode().splitlines():
                    metadata,name=entry.split('\t',1)
                    mode,entry_kind,entry_oid=metadata.split()
                    self.assertTrue(mode=='040000' or mode_ok(mode),'tree-entry:'+name+':'+mode)
            if kind=='blob':
                data=git('cat-file','blob',oid)
                if oid in png_objects:audit_png(data)
                else:self.inspect_text(data,'object:'+oid)
    def test_privacy_negative_controls(self):
        for parts in ((10,23,45,67),(192,168,12,34),(172,16,4,9),(172,31,8,2)):
            self.assertIsNotNone(PRIVATE_IP.search('.'.join(map(str,parts))))
        for parts in ((8,8,8,8),(172,32,4,9),(192,169,1,1)):
            self.assertIsNone(PRIVATE_IP.search('.'.join(map(str,parts))))
        for suffix in ('.pt','.pth','.onnx','.rknn','.tflite','.safetensors','.ckpt','.bin'):
            self.assertFalse(filename_ok('models/model'+suffix))
        for name in ('datasets/sample.txt','training/config.yaml','weights/checkpoint.txt','unlisted.py'):
            self.assertFalse(filename_ok(name))
        for word in ('TX'+'WQ','yh'+'wq','nc'+'nn'):
            self.assertFalse(content_ok('prefix '+word+' suffix'))
        self.assertTrue(content_ok('Independent CPU Canvas geometry demo'))
        self.assertTrue(filename_ok('port/godot3/rasterizer_cpu.cpp'))
        for mode in ('120000','160000','100120'):
            self.assertFalse(mode_ok(mode))
        for mode in ('100644','100755'):
            self.assertTrue(mode_ok(mode))
    def test_documentation_relative_links(self):
        for path in ROOT.rglob('*.md'):
            if '.git' in path.parts:continue
            for target in re.findall(r'!?\[[^\]]*\]\(([^)]+)\)',path.read_text(encoding='utf-8')):
                if target.startswith(('https://','http://','mailto:','#')):continue
                local=target.split('#',1)[0]
                resolved=(path.parent/local).resolve()
                self.assertTrue(ROOT==resolved or ROOT in resolved.parents,str(path)+':'+target)
                self.assertTrue(resolved.is_file(),str(path)+':'+target)
    def test_screenshot_scope(self):
        self.assertEqual(len(SCREENSHOT_DIMENSIONS),10)
        for name, dimensions in SCREENSHOT_DIMENSIONS.items():
            self.assertTrue(filename_ok(name))
            self.assertEqual(audit_png((ROOT/name).read_bytes(),name),dimensions)
        self.assertFalse(filename_ok('docs/images/screenshots/unreviewed.png'))
        scope=(ROOT/'LICENSES/SCREENSHOTS.md').read_text(encoding='utf-8')
        self.assertIn('does not relicense',scope)
        gallery=(ROOT/'docs/SCREENSHOTS.md').read_text(encoding='utf-8')
        for name in SCREENSHOT_DIMENSIONS:self.assertIn(Path(name).name,gallery)
        self.assertIn('not Android',gallery)
        self.assertIn('2026-10-15',gallery)
        self.assertIn('2026-10-16',gallery)
    def test_image_audit_negative_controls(self):
        import struct
        import zlib
        def chunk(kind,data):
            return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
        header=chunk(b'IHDR',struct.pack('>IIBBBBB',1,1,8,6,0,0,0))
        pixels=chunk(b'IDAT',zlib.compress(bytes((0,0,0,0,255))))
        end=chunk(b'IEND',b'')
        png=PNG_SIGNATURE+header+pixels+end
        self.assertEqual(audit_png(png),(1,1))
        for bad in (png+b'trailing',png[:-1],png[:40]+bytes((png[40]^1,))+png[41:],PNG_SIGNATURE+header+chunk(b'tEXt',b'private metadata')+pixels+end,PNG_SIGNATURE+header+chunk(b'IDAT',zlib.compress(bytes(1000000)))+end):
            with self.assertRaises(ValueError):audit_png(bad)
        with self.assertRaises(ValueError):audit_png(png,'docs/images/screenshots/rv-fixed-core.png')
        svg=b'<svg xmlns="http://www.w3.org/2000/svg"><rect width="1" height="1"/></svg>'
        self.assertTrue(audit_svg(svg))
        for element in ('<script/>','<image href="https://example.invalid/p.png"/>','<g onclick="run()"/>','<rect fill="url(https://example.invalid/f)"/>'):
            with self.assertRaises(ValueError):audit_svg(svg.replace(b'<rect width="1" height="1"/>',element.encode()))
        self.assertTrue(filename_ok('docs/images/architecture.png'))
        self.assertFalse(filename_ok('docs/images/unreviewed.png'))
if __name__=='__main__':unittest.main()
