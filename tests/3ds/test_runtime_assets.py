"""Optional full runtime extraction check using a caller-supplied private USA ROM."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[2]

class RuntimeAssetsTests(unittest.TestCase):
    def test_private_rom_runtime_extraction(self):
        rom=os.environ.get('SM64_TEST_ROM')
        if not rom: self.skipTest('SM64_TEST_ROM not supplied')
        rom=Path(rom).resolve()
        self.assertEqual(hashlib.sha1(rom.read_bytes()).hexdigest(),'9bef1128717f958171a4afac3ed78ee2bb4e86ce')
        with tempfile.TemporaryDirectory() as tmp:
            tmp=Path(tmp)
            runtime_rom=tmp/'baserom.us.z64'
            runtime_rom.write_bytes(rom.read_bytes())
            assets=json.loads((ROOT/'assets.json').read_text())
            source=['#include <stdio.h>\n#include <stdlib.h>\n#include <stdarg.h>\n#include "platform/3ds/runtime_assets.h"']
            count=0
            for path,info in assets.items():
                if path.startswith('@') or 'us' not in info[-1] or not all(isinstance(x,int) for x in info[-1]['us']): continue
                size,pos=info[-2],info[-1]['us'];kind=int(len(pos)>1);offset=pos[1] if kind else 0
                if path.startswith('textures/skyboxes/'):
                    size=64*2048;kind=3
                    source.append(f'static const unsigned char *p{count}[80];')
                source.append(f'static unsigned char a{count}[{size}]; REGISTER_ASSET(r{count},a{count},{size},{pos[0]},{offset},{kind},0);')
                if kind==3:
                    source.append(f'REGISTER_ASSET(s{count},p{count},320,{pos[0]},0,2,a{count});')
                count+=1
            source.append('''void diagnostics_log(const char *fmt,...) {va_list args;va_start(args,fmt);vfprintf(stderr,fmt,args);va_end(args);fputc(10,stderr);}
_Noreturn void diagnostics_fatal(const char *msg) {fprintf(stderr,"%s\\n",msg);exit(2);}
_Noreturn void diagnostics_errno(const char *msg,int code) {fprintf(stderr,"%s: %d\\n",msg,code);exit(3);}
extern unsigned char gSoundDataADSR[],gSoundDataRaw[];
int main(void) { runtime_assets_load();
FILE *f=fopen("audio.ctl","wb");fwrite(gSoundDataADSR,1,97856,f);fclose(f);
f=fopen("audio.tbl","wb");fwrite(gSoundDataRaw,1,2216704,f);fclose(f);return 0;}
''')
            (tmp/'test.c').write_text('\n'.join(source))
            subprocess.run(['gcc','-std=gnu11','-O1','-g','-fsanitize=address,undefined','-Isrc',f'-DSM64_ROM_PATH="{runtime_rom}"',str(tmp/'test.c'),
                'src/platform/3ds/runtime_assets.c','src/platform/3ds/rom_decode.c','src/platform/3ds/runtime_audio.c',
                'src/platform/3ds/asset_loader.c','-o',str(tmp/'test')],cwd=ROOT,check=True)
            subprocess.run([str(tmp/'test')],cwd=tmp,check=True)
            # TBL sample bytes must be bit-identical: ADPCM is never re-encoded.
            import struct
            data=rom.read_bytes()[5846368:5846368+2216704]
            converted=(tmp/'audio.tbl').read_bytes()
            entries=struct.unpack_from('>H',data,2)[0];end=4+entries*8
            self.assertEqual(data[end:],converted[end:])
            self.assertEqual(struct.unpack_from('<H',converted,2)[0],entries)
            for i in range(entries):
                self.assertEqual(struct.unpack_from('>II',data,4+i*8),struct.unpack_from('<II',converted,4+i*8))
            ctl=rom.read_bytes()[5748512:5846368]
            converted_ctl=(tmp/'audio.ctl').read_bytes()
            self.assertEqual(len(converted_ctl),97856)
            # Compare native fields with the repository's independent N64 parser.
            import importlib.util
            spec=importlib.util.spec_from_file_location('sound_reference',ROOT/'tools/disassemble_sound.py')
            ref=importlib.util.module_from_spec(spec);spec.loader.exec_module(ref)
            tbls,_,sample_map=ref.parse_tbl(data,ref.parse_seqfile(data,ref.TYPE_TBL))
            for index,(start,size) in enumerate(ref.parse_seqfile(ctl,ref.TYPE_CTL)):
                bank=ref.parse_ctl(ref.parse_ctl_header(ctl[start:start+16]),ctl[start+16:start+size],sample_map[tbls[index]],index,False)
                base=start+16
                def native(fmt,offset): return struct.unpack_from('<'+fmt,converted_ctl,base+offset)
                used_envelopes=set()
                for inst in bank.insts:
                    self.assertEqual(native('BBBBI',inst.addr),(0,inst.normal_range_lo,inst.normal_range_hi,inst.release_rate,inst.envelope))
                    used_envelopes.add(inst.envelope)
                    for n,sound in enumerate((inst.sound_lo,inst.sound_med,inst.sound_hi)):
                        self.assertEqual(native('If',inst.addr+8+n*8),tuple(sound) if sound else (0,0))
                for drum in bank.drums:
                    self.assertEqual(native('BBBBIfI',drum.addr),(drum.release_rate,drum.pan,0,0,*drum.sound,drum.envelope))
                    used_envelopes.add(drum.envelope)
                for offset in used_envelopes:
                    for n,entry in enumerate(bank.envelopes[offset].entries):
                        self.assertEqual(native('HH',offset+n*4),entry)
                for offset,sample in bank.samples.items():
                    zero,addr,loop,book,size=native('IIIII',offset)
                    self.assertEqual(zero,0)
                    self.assertEqual(native('IIi',loop),(sample.loop.start,sample.loop.end,sample.loop.count))
                    if sample.loop.state: self.assertEqual(native('16h',loop+16),sample.loop.state)
                    self.assertEqual(native('ii',book),(sample.book.order,sample.book.npredictors))
                    self.assertEqual(native(str(len(sample.book.table))+'h',book+8),tuple(sample.book.table))
            for fault,expected in [('missing','Runtime ROM fopen'),('short','read/size error'),('header','byte order'),('hash','SHA-1')]:
                payload=bytearray(rom.read_bytes())
                if fault=='short': payload=payload[:64]
                elif fault=='header': payload[0]=0
                elif fault=='hash': payload[-1]^=1
                if fault=='missing': runtime_rom.unlink()
                else: runtime_rom.write_bytes(payload)
                result=subprocess.run([str(tmp/'test')],cwd=tmp,capture_output=True,text=True)
                self.assertIn(result.returncode,(2,3),result.stderr)
                self.assertIn(expected,result.stderr)
            print(f'Runtime extraction exercised {count} ROM asset ranges under ASan/UBSan')
