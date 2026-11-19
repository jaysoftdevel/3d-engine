"""Run with Blender --background --python: generate game LODs from bundled CC0 scans."""
import bpy,bmesh
from pathlib import Path
root=Path(__file__).resolve().parents[2]/'sample-game/assets/showcase/scans'
for name in ['namaqualand_boulder_02','namaqualand_cliff_01','rock_07']:
 source=root/name/(name+'.gltf')
 for lod,target in [('near',10000 if name!='rock_07' else 5000),('far',900 if name!='rock_07' else 400)]:
  bpy.ops.object.select_all(action='SELECT');bpy.ops.object.delete(use_global=False)
  bpy.ops.import_scene.gltf(filepath=str(source))
  for ob in list(bpy.context.scene.objects):
   if ob.type!='MESH':continue
   bpy.context.view_layer.objects.active=ob;ob.select_set(True)
   bm=bmesh.new();bm.from_mesh(ob.data)
   # glTF splits vertices at UV/normal seams. Weld positions before decimation
   # so adjacent UV islands cannot drift apart and expose bright sky cracks.
   bmesh.ops.remove_doubles(bm,verts=list(bm.verts),dist=0.00001)
   # Keep scan undersides open; artificial cap faces have no scanned UVs.
   bm.normal_update();bm.to_mesh(ob.data);bm.free()
   tris=sum(len(p.vertices)-2 for p in ob.data.polygons)
   if tris>target:
    mod=ob.modifiers.new('Game LOD','DECIMATE');mod.ratio=target/tris
    bpy.ops.object.modifier_apply(modifier=mod.name)
   for p in ob.data.polygons:p.use_smooth=True
  path=root/(name+'-'+lod+'.glb')
  bpy.ops.export_scene.gltf(filepath=str(path),export_format='GLB',export_animations=False,export_cameras=False,export_lights=False,export_image_format='AUTO',export_yup=True)
  print(path.name,path.stat().st_size,flush=True)
