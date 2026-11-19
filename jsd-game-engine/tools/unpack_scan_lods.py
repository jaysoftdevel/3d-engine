"""Convert cooked GLBs to compact glTF geometry with shared CC0 image maps."""
from pathlib import Path
import json,struct,hashlib,io
from PIL import Image
root=Path(__file__).resolve().parents[2]/'sample-game/assets/showcase/scans'
tex=root/'textures';tex.mkdir(exist_ok=True)
for file in root.glob('*.glb'):
 data=file.read_bytes();n=struct.unpack_from('<I',data,12)[0];doc=json.loads(data[20:20+n]);binary=data[28+n:]
 image_views=set()
 for image in doc.get('images',[]):
  index=image.pop('bufferView');image_views.add(index);view=doc['bufferViews'][index]
  raw=binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']]
  decoded=Image.open(io.BytesIO(raw)).convert('RGB')
  key=hashlib.sha256(decoded.tobytes()).hexdigest()[:20];name=key+'.jpg'
  if not (tex/name).exists():decoded.save(tex/name,quality=96,subsampling=0,optimize=True)
  image['uri']='textures/'+name;image['mimeType']='image/jpeg'
 packed=bytearray();views=[];remap={}
 for i,view in enumerate(doc.get('bufferViews',[])):
  if i in image_views:continue
  while len(packed)%4:packed.append(0)
  offset=len(packed);packed.extend(binary[view.get('byteOffset',0):view.get('byteOffset',0)+view['byteLength']])
  remap[i]=len(views);views.append(dict(view,byteOffset=offset))
 for accessor in doc.get('accessors',[]):
  if 'bufferView'in accessor:accessor['bufferView']=remap[accessor['bufferView']]
  if 'sparse'in accessor:
   for side in ['indices','values']:accessor['sparse'][side]['bufferView']=remap[accessor['sparse'][side]['bufferView']]
 doc['bufferViews']=views;doc['buffers']=[{'byteLength':len(packed),'uri':file.stem+'.bin'}]
 doc['asset']['generator']='JSD offline LOD cooker / Blender / Poly Haven CC0 source'
 file.with_suffix('.gltf').write_text(json.dumps(doc,separators=(',',':')))
 file.with_suffix('.bin').write_bytes(packed)
 print(file.stem,len(packed))
print('Shared textures',sum(p.stat().st_size for p in tex.iterdir()))
