#!/usr/bin/env python3
"""Generate independent engine decoder fixtures using only Python's standard library."""
import base64, json, math, struct, wave, zlib
from pathlib import Path
root=Path(__file__).resolve().parent/'fixtures'
root.mkdir(parents=True,exist_ok=True)
def png(name,pixels,w=256,h=256):
    def chunk(tag,data):return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
    raw=b''.join(b'\0'+bytes(pixels[y*w*4:(y+1)*w*4]) for y in range(h))
    (root/name).write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b''))
base=[];normal=[];orm=[]
for y in range(256):
    for x in range(256):
        grid=x%64<3 or y%64<3;check=(x//32+y//32)%2
        base.extend((35 if grid else 85+check*55,50 if grid else 125+check*40,65 if grid else 155+check*35,255))
        nx=.35*math.sin(x*math.pi/16);ny=.35*math.sin(y*math.pi/16);nz=math.sqrt(1-nx*nx-ny*ny)
        normal.extend((int((nx*.5+.5)*255),int((ny*.5+.5)*255),int((nz*.5+.5)*255),255))
        orm.extend((255,70 if check else 170,180,255))
png('base-color.png',base);png('normal.png',normal);png('orm.png',orm)
points=[(-.7,0,-.5),(.7,0,-.5),(.7,2.4,-.5),(-.7,2.4,-.5),(-.7,0,.5),(.7,0,.5),(.7,2.4,.5),(-.7,2.4,.5)]
faces=[(1,0,3,2),(4,5,6,7),(0,4,7,3),(5,1,2,6),(3,7,6,2),(0,1,5,4)]
positions=[];normals=[];uvs=[];joints=[];weights=[]
for f in faces:
    a,b,c=[points[i] for i in f[:3]];ab=[b[i]-a[i] for i in range(3)];ac=[c[i]-a[i] for i in range(3)]
    n=[ab[1]*ac[2]-ab[2]*ac[1],ab[2]*ac[0]-ab[0]*ac[2],ab[0]*ac[1]-ab[1]*ac[0]];length=math.sqrt(sum(v*v for v in n));n=[v/length for v in n]
    for corner in (0,1,2,0,2,3):
        p=points[f[corner]];positions.extend(p);normals.extend(n);uvs.extend([(0,0),(1,0),(1,1),(0,1)][corner]);joints.extend((0,1,0,0));w=max(0,min(1,(p[1]-.8)/.8));weights.extend((1-w,w,0,0))
blob=bytearray();views=[];accessors=[]
def accessor(values,fmt,kind,components,minmax=False):
    while len(blob)%4:blob.append(0)
    offset=len(blob);blob.extend(struct.pack('<'+fmt*len(values),*values));views.append({'buffer':0,'byteOffset':offset,'byteLength':len(blob)-offset})
    a={'bufferView':len(views)-1,'componentType':5126 if fmt=='f' else 5123,'count':len(values)//components,'type':kind}
    if minmax:a['min']=[min(values[i::components]) for i in range(components)];a['max']=[max(values[i::components]) for i in range(components)]
    accessors.append(a);return len(accessors)-1
attributes={'POSITION':accessor(positions,'f','VEC3',3,True),'NORMAL':accessor(normals,'f','VEC3',3),'TEXCOORD_0':accessor(uvs,'f','VEC2',2),'JOINTS_0':accessor(joints,'H','VEC4',4),'WEIGHTS_0':accessor(weights,'f','VEC4',4)}
identity=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];tip=identity.copy();tip[13]=-1.2
bind=accessor(identity+tip,'f','MAT4',16);times=accessor([0,1,2,3],'f','SCALAR',1,True);q=[]
for angle in [0,.8,-.8,0]:q.extend((0,0,math.sin(angle/2),math.cos(angle/2)))
rotations=accessor(q,'f','VEC4',4)
gltf={'asset':{'version':'2.0','generator':'JSD engine decoder fixtures'},'buffers':[{'byteLength':len(blob),'uri':'data:application/octet-stream;base64,'+base64.b64encode(blob).decode()}],'bufferViews':views,'accessors':accessors,'images':[{'uri':n} for n in ['base-color.png','normal.png','orm.png']],'textures':[{'source':i} for i in range(3)],'materials':[{'pbrMetallicRoughness':{'baseColorTexture':{'index':0},'metallicRoughnessTexture':{'index':2},'metallicFactor':.6,'roughnessFactor':1},'normalTexture':{'index':1}}],'meshes':[{'primitives':[{'attributes':attributes,'material':0}]}],'nodes':[{'name':'rig','children':[1,3]},{'name':'base','children':[2]},{'name':'tip','translation':[0,1.2,0]},{'name':'skin','mesh':0,'skin':0}],'skins':[{'joints':[1,2],'inverseBindMatrices':bind}],'animations':[{'name':'wave','samplers':[{'input':times,'output':rotations,'interpolation':'LINEAR'}],'channels':[{'sampler':0,'target':{'node':2,'path':'rotation'}}]}],'scenes':[{'nodes':[0]}],'scene':0}
(root/'animated-column.gltf').write_text(json.dumps(gltf,separators=(',',':')))
with wave.open(str(root/'ambience.wav'),'wb') as f:
    f.setnchannels(1);f.setsampwidth(2);f.setframerate(48000)
    f.writeframes(b''.join(struct.pack('<h',int(1800*(math.sin(2*math.pi*110*t/48000)+.4*math.sin(2*math.pi*220*t/48000)))) for t in range(48000)))
w,h=128,64;hdr=bytearray(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 64 +X 128\n')
sun=(-.45,.8,.3);length=math.sqrt(sum(v*v for v in sun));sun=tuple(v/length for v in sun)
for y in range(h):
    row=[]
    for x in range(w):
        theta=(y+.5)/h*math.pi;phi=((x+.5)/w-.5)*2*math.pi;d=(math.sin(theta)*math.cos(phi),math.cos(theta),math.sin(theta)*math.sin(phi));bright=max(0,sum(a*b for a,b in zip(d,sun)))**400*100
        c=(.15+.5*max(0,d[1])+bright,.2+.65*max(0,d[1])+bright*.9,.25+.9*max(0,d[1])+bright*.7)
        exp=math.frexp(max(c))[1];scale=256*2**(-exp);row.append(tuple(int(v*scale) for v in c)+(exp+128,))
    hdr.extend((2,2,w>>8,w&255))
    for channel in range(4):hdr.append(128);hdr.extend(p[channel] for p in row)
(root/'sky.hdr').write_bytes(hdr)
print('Generated engine-owned PNG/glTF/WAV/HDR test fixtures:',root)
