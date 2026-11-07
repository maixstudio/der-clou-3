"""Builds the item props in Blender (headless) and exports them for Unreal.

  blender -b --factory-startup -P Art/Items/build_items.py -- [--textures] [--items desk,chair] [--preview] [--export]

--textures   regenerate the procedural textures (Art/Items/Textures)
--items      only these items (default: all)
--preview    render a preview per item and a contact sheet (Saved/ItemPreviews)
--export     write Art/Items/Export/SM_<id>.fbx
"""

import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import bpy  # noqa: E402
import numpy as np  # noqa: E402
from mathutils import Vector  # noqa: E402

import dc_items  # noqa: E402
import dc_materials  # noqa: E402
import dc_mesh  # noqa: E402
import dc_textures  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
TEX_DIR = os.path.join(HERE, "Textures")
EXPORT_DIR = os.path.join(HERE, "Export")
PREVIEW_DIR = os.path.join(ROOT, "Saved", "ItemPreviews")


def log(msg):
    print(f"[items] {msg}", flush=True)


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opts = {"textures": "--textures" in argv, "preview": "--preview" in argv, "export": "--export" in argv, "items": None}
    if "--items" in argv:
        opts["items"] = argv[argv.index("--items") + 1].split(",")
    return opts


def clear_scene():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    for me in list(bpy.data.meshes):
        bpy.data.meshes.remove(me)


# ---------------------------------------------------------------------------------------------
# Preview materials (Blender only â€“ Unreal builds its own from the same table)
# ---------------------------------------------------------------------------------------------


def node_of_type(nodes, type_name):
    return next(n for n in nodes if n.type == type_name)


def load_image(path, non_color):
    img = bpy.data.images.load(path, check_existing=True)
    img.colorspace_settings.name = "Non-Color" if non_color else "sRGB"
    return img


def build_materials():
    mats = {}
    for name, spec in dc_materials.MATERIALS.items():
        mat = bpy.data.materials.new(name)
        mat.use_nodes = True
        nt = mat.node_tree
        bsdf = node_of_type(nt.nodes, "BSDF_PRINCIPLED")
        tint = dc_materials.srgb_to_linear(spec["tint"]) + (1.0,)
        if spec.get("glass"):
            bsdf.inputs["Base Color"].default_value = tint
            bsdf.inputs["Roughness"].default_value = spec["rough"]
            bsdf.inputs["Alpha"].default_value = 0.18
            try:
                mat.surface_render_method = "BLENDED"
            except (AttributeError, TypeError):
                pass
            mats[name] = mat
            continue
        tex = spec["tex"]
        coord = nt.nodes.new("ShaderNodeTexCoord")
        mapping = nt.nodes.new("ShaderNodeMapping")
        mapping.inputs["Scale"].default_value = (spec["tiling"], spec["tiling"], 1.0)
        nt.links.new(coord.outputs["UV"], mapping.inputs["Vector"])

        def tex_node(suffix, non_color):
            n = nt.nodes.new("ShaderNodeTexImage")
            n.image = load_image(os.path.join(TEX_DIR, f"T_{tex}_{suffix}.png"), non_color)
            nt.links.new(mapping.outputs["Vector"], n.inputs["Vector"])
            return n

        bc, orm, nrm = tex_node("BC", False), tex_node("ORM", True), tex_node("N", True)
        tint_node = nt.nodes.new("ShaderNodeMix")
        tint_node.data_type = "RGBA"
        tint_node.blend_type = "MULTIPLY"
        tint_node.inputs[0].default_value = 1.0
        nt.links.new(bc.outputs["Color"], tint_node.inputs[6])
        tint_node.inputs[7].default_value = tint
        nt.links.new(tint_node.outputs[2], bsdf.inputs["Base Color"])
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(orm.outputs["Color"], sep.inputs["Color"])
        rough = nt.nodes.new("ShaderNodeMath")
        rough.operation = "MULTIPLY"
        rough.use_clamp = True
        nt.links.new(sep.outputs[1], rough.inputs[0])
        rough.inputs[1].default_value = spec["rough"]
        nt.links.new(rough.outputs[0], bsdf.inputs["Roughness"])
        metal = nt.nodes.new("ShaderNodeMath")
        metal.operation = "MULTIPLY"
        nt.links.new(sep.outputs[2], metal.inputs[0])
        metal.inputs[1].default_value = spec["metal"]
        nt.links.new(metal.outputs[0], bsdf.inputs["Metallic"])
        # DirectX normal map -> Blender (OpenGL): flip green.
        sep_n = nt.nodes.new("ShaderNodeSeparateColor")
        nt.links.new(nrm.outputs["Color"], sep_n.inputs["Color"])
        inv = nt.nodes.new("ShaderNodeMath")
        inv.operation = "SUBTRACT"
        inv.inputs[0].default_value = 1.0
        nt.links.new(sep_n.outputs[1], inv.inputs[1])
        comb = nt.nodes.new("ShaderNodeCombineColor")
        nt.links.new(sep_n.outputs[0], comb.inputs[0])
        nt.links.new(inv.outputs[0], comb.inputs[1])
        nt.links.new(sep_n.outputs[2], comb.inputs[2])
        nmap = nt.nodes.new("ShaderNodeNormalMap")
        nt.links.new(comb.outputs[0], nmap.inputs["Color"])
        nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])
        if spec.get("emissive"):
            bsdf.inputs["Emission Color"].default_value = tint
            bsdf.inputs["Emission Strength"].default_value = spec["emissive"] * 0.5
        mats[name] = mat
    return mats


# ---------------------------------------------------------------------------------------------
# Preview rendering
# ---------------------------------------------------------------------------------------------


def setup_render_scene():
    scene = bpy.context.scene
    try:
        scene.render.engine = "BLENDER_EEVEE"
    except TypeError as err:
        log(f"engine: {err}")
    for attr, value in (("use_raytracing", True), ("use_shadows", True), ("taa_render_samples", 32)):
        try:
            setattr(scene.eevee, attr, value)
        except (AttributeError, TypeError):
            pass
    scene.render.resolution_x = 560
    scene.render.resolution_y = 560
    scene.render.film_transparent = False
    try:
        scene.view_settings.view_transform = "AgX"
    except TypeError:
        pass
    # World: soft studio gradient.
    world = bpy.data.worlds.new("Studio")
    scene.world = world
    world.use_nodes = True
    nt = world.node_tree
    bg = node_of_type(nt.nodes, "BACKGROUND")
    coord = nt.nodes.new("ShaderNodeTexCoord")
    sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    nt.links.new(coord.outputs["Generated"], sep.inputs["Vector"])
    nt.links.new(sep.outputs["Z"], ramp.inputs["Fac"])
    ramp.color_ramp.elements[0].position = 0.45
    ramp.color_ramp.elements[0].color = (0.05, 0.05, 0.055, 1)
    ramp.color_ramp.elements[1].position = 0.75
    ramp.color_ramp.elements[1].color = (0.9, 0.92, 1.0, 1)
    nt.links.new(ramp.outputs["Color"], bg.inputs["Color"])
    bg.inputs["Strength"].default_value = 0.35
    # Key light and rim light.
    key = bpy.data.lights.new("Key", "AREA")
    key.energy = 900
    key.size = 3
    key_ob = bpy.data.objects.new("Key", key)
    scene.collection.objects.link(key_ob)
    rim = bpy.data.lights.new("Rim", "AREA")
    rim.energy = 500
    rim.size = 2
    rim_ob = bpy.data.objects.new("Rim", rim)
    scene.collection.objects.link(rim_ob)
    # Floor.
    floor_mat = bpy.data.materials.new("PreviewFloor")
    floor_mat.use_nodes = True
    fb = node_of_type(floor_mat.node_tree.nodes, "BSDF_PRINCIPLED")
    fb.inputs["Base Color"].default_value = (0.32, 0.30, 0.28, 1)
    fb.inputs["Roughness"].default_value = 0.7
    floor_me = bpy.data.meshes.new("Floor")
    floor_me.from_pydata([(-50, -50, 0), (50, -50, 0), (50, 50, 0), (-50, 50, 0)], [], [(0, 1, 2, 3)])
    floor_me.materials.append(floor_mat)
    floor = bpy.data.objects.new("Floor", floor_me)
    scene.collection.objects.link(floor)
    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 50
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return key_ob, rim_ob, cam


def frame(ob, key_ob, rim_ob, cam):
    corners = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    lo = Vector([min(c[i] for c in corners) for i in range(3)])
    hi = Vector([max(c[i] for c in corners) for i in range(3)])
    centre = (lo + hi) / 2
    radius = max((hi - lo).length / 2, 0.02)
    fov = cam.data.angle
    direction = Vector((0.9, -1.5, 0.95)).normalized()
    dist = radius / math.sin(fov / 2) * 1.05
    cam.location = centre + direction * dist
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.clip_start = dist * 0.01
    cam.data.clip_end = dist * 10
    key_ob.location = centre + Vector((-1.2, -1.6, 2.2)).normalized() * radius * 6
    key_ob.rotation_euler = (centre - key_ob.location).to_track_quat("-Z", "Y").to_euler()
    key_ob.data.size = radius * 3
    key_ob.data.energy = 45 * (radius * 6) ** 2
    rim_ob.location = centre + Vector((1.5, 1.8, 1.2)).normalized() * radius * 6
    rim_ob.rotation_euler = (centre - rim_ob.location).to_track_quat("-Z", "Y").to_euler()
    rim_ob.data.size = radius * 2
    rim_ob.data.energy = 25 * (radius * 6) ** 2


def contact_sheet(paths, out_path, cols=5):
    tiles = []
    for p in paths:
        img = bpy.data.images.load(p)
        w, h = img.size
        px = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(px)
        tiles.append(px.reshape(h, w, 4))
        bpy.data.images.remove(img)
    if not tiles:
        return
    h, w = tiles[0].shape[:2]
    rows = (len(tiles) + cols - 1) // cols
    sheet = np.zeros((rows * h, cols * w, 4), dtype=np.float32)
    sheet[..., 3] = 1
    for i, t in enumerate(tiles):
        r, c = divmod(i, cols)
        sheet[(rows - 1 - r) * h:(rows - r) * h, c * w:(c + 1) * w] = t
    img = bpy.data.images.new("sheet", cols * w, rows * h)
    img.pixels.foreach_set(sheet.ravel())
    img.filepath_raw = out_path
    img.file_format = "PNG"
    img.save()


# ---------------------------------------------------------------------------------------------


def export_fbx(ob, path):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types={"MESH"}, apply_unit_scale=True,
                             apply_scale_options="FBX_SCALE_UNITS", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             use_tspace=True, add_leaf_bones=False, bake_anim=False, axis_forward="-Z", axis_up="Y",
                             path_mode="STRIP")


def main():
    opts = parse_args()
    if opts["textures"] or not os.path.exists(os.path.join(TEX_DIR, "T_Fine_BC.png")):
        dc_textures.build_all(TEX_DIR, log=log)
    clear_scene()
    mats = build_materials()
    ids = opts["items"] or list(dc_items.BUILDERS)
    os.makedirs(EXPORT_DIR, exist_ok=True)
    os.makedirs(PREVIEW_DIR, exist_ok=True)
    rig = setup_render_scene() if opts["preview"] else None
    previews = []
    for item_id in ids:
        builder = dc_items.BUILDERS.get(item_id)
        if not builder:
            log(f"unknown item {item_id}")
            continue
        it = dc_mesh.Item(item_id, mats)
        builder(it)
        ob = it.join()
        corners = [Vector(c) for c in ob.bound_box]
        size = [round((max(c[i] for c in corners) - min(c[i] for c in corners)) * 100, 1) for i in range(3)]
        log(f"{item_id}: {len(ob.data.polygons)} faces, size cm {size}, materials {[m.name if m else None for m in ob.data.materials]}")
        if opts["export"]:
            export_fbx(ob, os.path.join(EXPORT_DIR, f"SM_{item_id}.fbx"))
        if rig:
            frame(ob, *rig)
            path = os.path.join(PREVIEW_DIR, f"{item_id}.png")
            bpy.context.scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            previews.append(path)
        ob.hide_render = True
        ob.hide_set(True)
    if previews:
        contact_sheet(previews, os.path.join(PREVIEW_DIR, "_sheet.png"))
        log(f"previews in {PREVIEW_DIR}")


if __name__ == "__main__":
    main()
