#!/usr/bin/env python3
"""Generate ROM-free asset declarations/offsets from the repository manifest.
No ROM, PNG, AIFF or extracted data is read. The only binary input is the sound
player bytecode assembled from repository source.
"""
import argparse
import json
import re
import struct
from pathlib import Path

def registry(name, dest, size, rom, offset, kind, base='0'):
    return f'REGISTER_ASSET({name}, {dest}, {size}, {rom}, {offset}, {kind}, {base});\n'

def generate(root):
    root = Path(root)
    assets = json.loads(Path('assets.json').read_text())
    def write(path, text):
        p=root/path; p.parent.mkdir(parents=True,exist_ok=True); p.write_text(text)
    prefix='#include "types.h"\n#include "platform/3ds/runtime_assets.h"\n'
    for path, info in assets.items():
        if path.startswith('@') or 'us' not in info[-1]: continue
        size,pos=info[-2],info[-1]['us']
        if path.startswith('textures/skyboxes/'):
            name=Path(path).stem; rom=pos[0]; length=64*2048
            text=prefix+f'static unsigned char {name}_data[{length}];\nconst Texture *{name}_skybox_ptrlist[80];\n'
            text+=registry(name+'_data_asset',name+'_data',length,rom,0,3)
            text+=registry(name+'_ptr_asset',name+'_skybox_ptrlist',320,rom,0,2,name+'_data')
            write(Path('bin')/(name+'_skybox.c'),text)
        elif path=='levels/ending/cake.png':
            text=''
            for i in range(48):
                text+=f'static const Texture cake_end_texture_{i}[] = {{ SM64_ASSET(3200,{pos[0]},{i*3200},1) }};\n'
            write(Path('levels/ending/cake.inc.c'),text)
        elif path.endswith('.png'):
            if not all(isinstance(p,int) for p in pos): raise ValueError(path)
            write(Path(path[:-4]+'.inc.c'),f'SM64_ASSET({size},{pos[0]},{pos[1] if len(pos)>1 else 0},{1 if len(pos)>1 else 0})\n')
    description=json.loads(re.sub(r'/\*.*?\*/','',Path('assets/demo_data.json').read_text(),flags=re.S))
    selected=lambda items:[x for x in items if 'ifdef' not in x or 'VERSION_US' in x['ifdef']]
    demos=selected(description['demofiles']); table=selected(description['table'])
    text='#include "game/memory.h"\n#include <stddef.h>\n#include "platform/3ds/runtime_assets.h"\n'
    text+='struct DemoInputsObj { u32 count; void *reserved; struct OffsetSizePair entries[%d];\n'%len(table)
    for item in demos:
        name=item['name'];text+=f'unsigned char {name}[{assets["assets/demos/"+name+".bin"][-2]}];\n'
    text+='} gDemoInputs = { .count=%d, .entries={\n'%len(table)
    for item in table:
        name=item['demofile'];text+=f'{{offsetof(struct DemoInputsObj,{name}),sizeof(gDemoInputs.{name})+{item.get("extraSize",0)}}},\n'
    text+='}};\n'
    for item in demos:
        name=item['name'];info=assets['assets/demos/'+name+'.bin'];text+=registry('demo_'+name,'gDemoInputs.'+name,info[-2],info[-1]['us'][0],0,0)
    write(Path('assets/demo_data.c'),text)
    # Sequence structure has native-endian offsets and unmodified bytecode.
    seqs=json.loads(Path('sound/sequences.json').read_text());seqs.pop('comment',None)
    seqs={k:(v.get('banks') if 'VERSION_US' in v.get('ifdef',[]) else None) if isinstance(v,dict) else v for k,v in seqs.items()}
    names=sorted(seqs);names=[n for n in names if int(n[:2],16)<=0x22]
    player=(root/'sound/sequences/00_sound_player.m64').read_bytes()
    count=max(int(n[:2],16) for n in names)+1
    align=lambda n:(n+15)&~15
    cursor=align(4+count*8); headers=bytearray(cursor);struct.pack_into('<HH',headers,0,0,count)
    descriptors=[]; initial={}
    for name in names:
        index=int(name[:2],16)
        if not seqs[name]: continue
        if index==0:
            data=player;length=len(data)
            for i,b in enumerate(data):
                if b: initial[cursor+i]=b
        else:
            key=next(k for k in assets if k.endswith('/'+name+'.m64') and 'us' in assets[k][-1])
            length=assets[key][-2];descriptors.append(registry('seq_'+str(index),'gMusicData+'+str(cursor),length,assets[key][-1]['us'][0],0,0))
        struct.pack_into('<II',headers,4+index*8,cursor,length);cursor=align(cursor+length)
    for i,b in enumerate(headers):
        if b: initial[i]=b
    text=prefix+f'unsigned char gMusicData[{cursor}]={{'+','.join(f'[{i}]={v}' for i,v in sorted(initial.items()))+'};\n'+''.join(descriptors)
    banks=sorted(p.stem for p in Path('sound/sound_banks').glob('*.json'))
    sets=bytearray(count*2)
    for name in names:
        struct.pack_into('<H',sets,int(name[:2],16)*2,len(sets));bankset=seqs[name] or []
        sets.extend([len(bankset)]+[banks.index(b) for b in bankset[::-1]])
    sets.extend(bytes(max(256,align(len(sets)))-len(sets)))
    text+='unsigned char gBankSetsData[%d]={%s};\n'%(len(sets),','.join(map(str,sets)))
    write(Path('sound/runtime_sequences.c'),text)

def transform(source,destination):
    text=Path(source).read_text()
    text=re.sub(r'^#.*\n','',text,flags=re.M)
    pattern=r'(?:^|(?<=[;{}]))([^;{}]*?\b(\w+)\s*)\[[^\]]*\]\s*=\s*\{\s*SM64_ASSET\(([^)]*)\)\s*\};'
    count=0
    def replace(m):
        nonlocal count
        declaration,name,args=m.groups();size,rom,offset,kind=map(int,args.split(','))
        if not re.search(r'\b(Texture|u8)\b',declaration): raise ValueError('Unsupported asset declaration: '+declaration)
        declaration=re.sub(r'\bconst\s+','',declaration)
        out=declaration+f'[{size}];\n'+registry('sm64_asset_'+str(count),name,size,rom,offset,kind)
        count+=1
        return out
    text=re.sub(pattern,replace,text)
    if 'SM64_ASSET(' in text: raise ValueError('Unconverted asset initializer in '+source)
    # Include after preprocessing: it defines the descriptor for injected entries.
    Path(destination).write_text('#include "platform/3ds/runtime_asset_registry.h"\n'+text)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generate');parser.add_argument('--transform',nargs=2)
    args=parser.parse_args()
    if args.generate: generate(args.generate)
    elif args.transform: transform(*args.transform)
    else: parser.error('choose --generate or --transform')
