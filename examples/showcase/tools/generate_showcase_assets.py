#!/usr/bin/env python3
"""Original physically-based showcase maps and UI artwork. Offline, NumPy/Pillow/SciPy."""
from pathlib import Path
import numpy as np
from PIL import Image,ImageDraw,ImageFont
from scipy.ndimage import gaussian_filter,zoom
import struct,math
root=Path(__file__).resolve().parents[1]/'assets/showcase'
root.mkdir(parents=True,exist_ok=True)
import argparse
parser=argparse.ArgumentParser();parser.add_argument("--size",type=int,default=512);args=parser.parse_args()
size=args.size;rng=np.random.default_rng(8147)
y,x=np.mgrid[0:size,0:size].astype(np.float32);x/=size;y/=size

def field(scale):
 a=rng.normal(size=(scale,scale)).astype(np.float32)
 a=zoom(a,size/scale,order=3,mode='wrap')[:size,:size]
 a=(a-a.mean())/max(a.std(),.001)
 return a
lo=field(8);mid=field(32);fine=field(128);micro=rng.normal(size=(size,size)).astype(np.float32)

def maps(name,color,height,roughness,metallic=0,ao=None,strength=1):
 rgb=np.clip(color,0,1)
 if rgb.ndim==1:rgb=np.broadcast_to(rgb,(size,size,3)).copy()
 Image.fromarray((rgb*255).astype(np.uint8)).save(root/(name+'-color.png'))
 dx=(np.roll(height,-1,axis=1)-np.roll(height,1,axis=1))*strength*size/1024
 dy=(np.roll(height,-1,axis=0)-np.roll(height,1,axis=0))*strength*size/1024
 n=np.stack((-dx,-dy,np.ones_like(dx)),axis=-1);n/=np.linalg.norm(n,axis=-1,keepdims=True)
 Image.fromarray(np.clip((n*.5+.5)*255,0,255).astype(np.uint8)).save(root/(name+'-normal.png'))
 r=np.clip(np.broadcast_to(roughness,(size,size)),.08,1)
 a=np.ones((size,size)) if ao is None else np.clip(ao,0,1)
 m=np.broadcast_to(metallic,(size,size))
 Image.fromarray((np.stack((a,r,m),axis=-1)*255).astype(np.uint8)).save(root/(name+'-orm.png'))
# Wind-rippled grains with irregular darker mineral patches, not a checkerboard.
ripple=np.sin((x*34+y*8+lo*.08)*np.pi*2)
h=lo*.12+mid*.035+ripple*.014+fine*.013+micro*.006
variation=1+lo*.08+mid*.04+fine*.024+micro*.025
sand=np.array([.51,.405,.285])[None,None,:]*variation[...,None]
minerals=np.clip((fine-1.8)*.15,0,.35);sand-=minerals[...,None]
maps('sand',sand,h,.89+mid*.025,ao=.95+np.minimum(fine,0)*.02,strength=12)
# Sandstone stratification plus broad eroded fractures and granular detail.
strata=np.sin((y*23+lo*.032+mid*.009)*np.pi*2)
crack=np.clip((.10-np.abs(mid*.34+lo*.12))*9,0,1)
rock=np.array([.55,.43,.315])[None,None,:]*(1+lo[...,None]*.11+strata[...,None]*.04+fine[...,None]*.035)
rock*=1-crack[...,None]*.28
h=lo*.32+mid*.12+strata*.018+fine*.026+micro*.005-crack*.08
maps('rock',rock,h,.8+fine*.035,ao=.95-crack*.22,strength=11)
# Aged powder-coated industrial metal with fine chips, machining scratches and oil staining.
chip=gaussian_filter((fine>1.85).astype(float),.7)
scratches=np.zeros((size,size),dtype=np.float32)
for i in range(180):
 xx=int(rng.integers(0,size));yy=int(rng.integers(0,size));length=int(rng.integers(5,90));scratches[yy:min(size,yy+length),xx:xx+1]=rng.uniform(.1,.7)
for name,base in [('paint',[.58,.195,.062]),('steel',[.255,.285,.285]),('ivory',[.72,.68,.54]),('dark',[.065,.078,.084])]:
 c=np.array(base)[None,None,:]*(1+lo[...,None]*.045+mid[...,None]*.025+fine[...,None]*.018)
 c=c*(1-chip[...,None])+np.array([.32,.30,.26])*chip[...,None]
 c+=scratches[...,None]*.12
 metal=(.16+chip*.75) if name in ('paint','ivory') else .82+np.zeros((size,size))
 maps(name,c,mid*.012+fine*.008+micro*.004-scratches*.02,.48+mid*.035+fine*.018+chip*.12,metal,ao=.98-chip*.12,strength=6)
rubber=np.array([.063,.067,.063])[None,None,:]*(1+fine[...,None]*.07+mid[...,None]*.06)
maps('rubber',rubber,mid*.012+micro*.009,.82+fine*.025,strength=7)
concrete=np.array([.40,.405,.38])[None,None,:]*(1+lo[...,None]*.06+fine[...,None]*.08)
maps('concrete',concrete,mid*.04+fine*.015+micro*.006,.9,ao=.96+np.minimum(fine,0)*.02,strength=10)
fontroot=Path('/usr/share/fonts/truetype/dejavu')
def font(n,bold=False,mono=False):
 name='DejaVuSansMono' if mono else 'DejaVuSans'
 if bold:name+='-Bold'
 try:return ImageFont.truetype(str(fontroot/(name+'.ttf')),n)
 except OSError:return ImageFont.load_default()
# Physical signage carries scale and purpose; it is a material texture, not a rendered scene.
sign=Image.new('RGBA',(1536,384),(23,32,33,255));d=ImageDraw.Draw(sign)
d.rectangle((20,20,1516,364),outline=(161,131,73),width=6)
d.text((65,36),'KESTREL',font=font(120,True),fill=(217,211,180))
d.text((70,196),'REMOTE RELAY  /  SECTOR 08',font=font(39,mono=True),fill=(144,176,169))
d.text((1130,60),'04',font=font(170,True),fill=(215,133,55))
d.line((70,278,1460,278),fill=(161,131,73),width=3)
d.text((70,300),'CAUTION  /  AUTOMATED CARGO SYSTEM  /  KEEP CLEAR',font=font(28,mono=True),fill=(198,183,145))
sign.save(root/'relay-sign.png')
for name,title,sub in [('title','S U N D O G','RECOVERY AT KESTREL RELAY'),('mission','RECOVER. REPAIR. RECONNECT.','AUTONOMOUS EXPEDITION  /  SECTOR 08'),('credits','RELAY ONLINE','EXPEDITION COMPLETE  /  JSD GAME ENGINE')]:
 im=Image.new('RGBA',(1280,260),(0,0,0,0));d=ImageDraw.Draw(im)
 d.text((4,8),title,font=font(74,True),fill=(236,232,213,255),stroke_width=1)
 d.line((8,112,430,112),fill=(208,136,66),width=3)
 d.text((8,138),sub,font=font(23,mono=True),fill=(194,202,196,255))
 im.save(root/(name+'.png'))
# Linear HDR irradiance environment, aligned with the showcase's sun.
w,h=512,256;yy,xx=np.mgrid[:h,:w];phi=(xx+.5)/w*2*np.pi-np.pi;theta=(yy+.5)/h*np.pi
D=np.stack((np.cos(phi)*np.sin(theta),np.cos(theta),np.sin(phi)*np.sin(theta)),axis=-1)
sun=np.array([-.48,.59,.65]);sun/=np.linalg.norm(sun);horizon=np.clip(1-D[...,1],0,1)**4
sky=np.array([.055,.15,.29])*(1-horizon[...,None])+np.array([.65,.61,.48])*horizon[...,None]
below=np.clip(-D[...,1]*5,0,1)[...,None];sky=sky*(1-below)+np.array([.16,.125,.085])*below
angle=np.einsum('ijk,k->ij',D,sun);sky+=np.array([1,.83,.58])*np.maximum(angle,0)[...,None]**32*.25
sky+=np.array([1,.82,.55])*((angle>np.cos(.012))[...,None])*35
# Radiance RGBE RLE format.
with (root/'daylight.hdr').open('wb') as f:
 f.write(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 256 +X 512\n')
 peak=np.maximum(sky.max(axis=-1),1e-20);mantissa,exponent=np.frexp(peak);scale=mantissa*256/peak
 rgbe=np.concatenate((np.clip(sky*scale[...,None],0,255).astype(np.uint8),np.clip(exponent+128,0,255).astype(np.uint8)[...,None]),axis=-1)
 for row in rgbe:
  f.write(bytes((2,2,w>>8,w&255)))
  for c in range(4):
   line=row[:,c].tobytes()
   for at in range(0,w,128):chunk=line[at:at+128];f.write(bytes((len(chunk),)));f.write(chunk)
print(f'Generated {len(list(root.iterdir()))} original showcase assets in {root}')
