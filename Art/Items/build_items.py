"""Builds the item props in Blender (headless) and exports them for Unreal.

  blender -b --factory-startup -P Art/Items/build_items.py -- [--textures] [--items desk,chair] [--preview] [--export]

--textures   regenerate the procedural textures (Art/Items/Textures)
--items      only these items (default: all)
--preview    render two previews per item (<id>.png three-quarter view, <id>_top.png steep view like the game
             camera) and a contact sheet (Saved/ItemPreviews)
--sheet      name of the contact sheet: _sheet_<name>.png (default _sheet.png) – use one per parallel run
--ref        put a grey 180 cm figure next to items taller than 40 cm (preview only, never exported)
--export     write Art/Items/Export/SM_<id>.fbx
--icons      render one inventory icon per item (transparent, orthographic, Saved/ItemIcons/raw/<id>.png);
             Scripts/Tools/prepare_item_icons.py then fits them to the grid footprint for the game
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
ICON_DIR = os.path.join(ROOT, "Saved", "ItemIcons", "raw")


def log(msg):
    print(f"[items] {msg}", flush=True)


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    opts = {"textures": "--textures" in argv, "preview": "--preview" in argv, "export": "--export" in argv, "items": None,
            "sheet": None, "ref": "--ref" in argv, "icons": "--icons" in argv}
    if "--items" in argv:
        opts["items"] = argv[argv.index("--items") + 1].split(",")
    if "--sheet" in argv:
        opts["sheet"] = argv[argv.index("--sheet") + 1]
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


VIEW_THREE_QUARTER = (0.9, -1.5, 0.95)
VIEW_TOP = (0.25, -0.55, 1.5)  # steep, roughly like the game camera


def frame(obs, key_ob, rim_ob, cam, view=VIEW_THREE_QUARTER):
    corners = [o.matrix_world @ Vector(c) for o in obs for c in o.bound_box]
    lo = Vector([min(c[i] for c in corners) for i in range(3)])
    hi = Vector([max(c[i] for c in corners) for i in range(3)])
    centre = (lo + hi) / 2
    radius = max((hi - lo).length / 2, 0.02)
    fov = cam.data.angle
    direction = Vector(view).normalized()
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


# ---------------------------------------------------------------------------------------------
# Inventory icons: same item from above-front-right, no floor, transparent, lit to read on dark tiles
# ---------------------------------------------------------------------------------------------

VIEW_ICON = (0.75, -1.35, 1.35)
# Items that read better from elsewhere: the painting's canvas faces -X, the flat pick case needs a steep look inside.
ICON_VIEWS = {
    "painting": (-1.4, -0.7, 0.55),
    "lockpicks": (0.35, -0.6, 1.6),
}
ICON_PIXELS = 512


def setup_icon_scene():
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
    scene.render.resolution_x = ICON_PIXELS
    scene.render.resolution_y = ICON_PIXELS
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    try:
        scene.view_settings.view_transform = "AgX"
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass
    scene.view_settings.exposure = -0.3
    # Even grey surround so dark materials keep their shape.
    world = bpy.data.worlds.new("IconStudio")
    scene.world = world
    world.use_nodes = True
    bg = node_of_type(world.node_tree.nodes, "BACKGROUND")
    bg.inputs["Color"].default_value = (0.55, 0.58, 0.64, 1)
    bg.inputs["Strength"].default_value = 0.3
    lights = []
    # Warm key from front-left-top, cool rim from behind-right (like the game's lamps and moon), soft fill from the camera.
    for name, energy, colour, direction, size in (("IconKey", 60, (1.0, 0.92, 0.82), (-1.1, -1.5, 2.0), 3.0),
                                                  ("IconRim", 55, (0.75, 0.85, 1.0), (1.6, 1.6, 1.1), 2.0),
                                                  ("IconFill", 18, (1.0, 1.0, 1.0), VIEW_ICON, 4.0)):
        light = bpy.data.lights.new(name, "AREA")
        light.color = colour
        ob = bpy.data.objects.new(name, light)
        scene.collection.objects.link(ob)
        lights.append((ob, energy, Vector(direction).normalized(), size))
    cam_data = bpy.data.cameras.new("IconCam")
    cam_data.type = "ORTHO"
    cam = bpy.data.objects.new("IconCam", cam_data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    return lights, cam


def frame_icon(ob, lights, cam, view=VIEW_ICON):
    corners = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    lo = Vector([min(c[i] for c in corners) for i in range(3)])
    hi = Vector([max(c[i] for c in corners) for i in range(3)])
    centre = (lo + hi) / 2
    radius = max((hi - lo).length / 2, 0.01)
    direction = Vector(view).normalized()
    cam.location = centre + direction * radius * 4
    cam.rotation_euler = (centre - cam.location).to_track_quat("-Z", "Y").to_euler()
    cam.data.ortho_scale = radius * 2.1
    cam.data.clip_start = radius * 0.1
    cam.data.clip_end = radius * 10
    for light_ob, energy, light_dir, size in lights:
        light_ob.location = centre + light_dir * radius * 6
        light_ob.rotation_euler = (centre - light_ob.location).to_track_quat("-Z", "Y").to_euler()
        light_ob.data.size = radius * size
        light_ob.data.energy = energy * (radius * 6) ** 2


def reference_figure(ob):
    """Grey 180 cm stand-in person beside the item, for judging proportions in the preview."""
    corners = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
    hi_x = max(c.x for c in corners)
    mat = bpy.data.materials.get("RefFigure") or bpy.data.materials.new("RefFigure")
    mat.use_nodes = True
    node_of_type(mat.node_tree.nodes, "BSDF_PRINCIPLED").inputs["Base Color"].default_value = (0.35, 0.36, 0.40, 1)
    parts = []
    for name, op, kw in (("RefBody", bpy.ops.mesh.primitive_cylinder_add, dict(radius=0.2, depth=1.55, location=(0, 0, 0.775))),
                         ("RefHead", bpy.ops.mesh.primitive_uv_sphere_add, dict(radius=0.12, location=(0, 0, 1.68)))):
        op(**kw)
        o = bpy.context.active_object
        o.name = name
        o.data.materials.append(mat)
        o.location.x += hi_x + 0.45
        parts.append(o)
    return parts


def contact_sheet(paths, out_path, cols=4):
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
    if opts["icons"] and opts["preview"]:
        log("--icons and --preview need different scenes: run them one after the other")
        return
    rig = setup_render_scene() if opts["preview"] else None
    icon_rig = setup_icon_scene() if opts["icons"] else None
    if icon_rig:
        os.makedirs(ICON_DIR, exist_ok=True)
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
            ref = reference_figure(ob) if opts["ref"] and size[2] > 40 else []
            for suffix, view in (("", VIEW_THREE_QUARTER), ("_top", VIEW_TOP)):
                frame([ob] + ref, *rig, view=view)
                path = os.path.join(PREVIEW_DIR, f"{item_id}{suffix}.png")
                bpy.context.scene.render.filepath = path
                bpy.ops.render.render(write_still=True)
                previews.append(path)
            for o in ref:
                bpy.data.objects.remove(o, do_unlink=True)
        if icon_rig:
            frame_icon(ob, *icon_rig, view=ICON_VIEWS.get(item_id, VIEW_ICON))
            path = os.path.join(ICON_DIR, f"{item_id}.png")
            bpy.context.scene.render.filepath = path
            bpy.ops.render.render(write_still=True)
            previews.append(path)
        ob.hide_render = True
        ob.hide_set(True)
    if previews:
        name = f"_sheet_{opts['sheet']}.png" if opts["sheet"] else ("_sheet_icons.png" if icon_rig else "_sheet.png")
        contact_sheet(previews, os.path.join(PREVIEW_DIR, name), cols=8 if icon_rig else 4)
        log(f"{'icons in ' + ICON_DIR if icon_rig else 'previews in ' + PREVIEW_DIR} ({name})")


if __name__ == "__main__":
    main()
