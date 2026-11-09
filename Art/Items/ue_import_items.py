"""Unreal side of the item pipeline (run as commandlet, editor closed):

  UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript -script="Art/Items/ue_import_items.py" -unattended

Imports Art/Items/Textures and Art/Items/Export into /Game/DerClou/Items, builds the master
materials M_ItemSurface / M_ItemGlass, one instance MI_Item_<Name> per entry of dc_materials and
assigns them to the mesh slots by slot name. Every mesh gets a simple box collision.
"""

import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import dc_materials  # noqa: E402

BASE = "/Game/DerClou/Items"
TEX_PATH, MAT_PATH, MESH_PATH = BASE + "/Textures", BASE + "/Materials", BASE + "/Meshes"
TEX_DIR = os.path.join(HERE, "Textures")
EXPORT_DIR = os.path.join(HERE, "Export")

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning(f"[items] {msg}")


def import_files(files, dest):
    tasks = []
    for f in files:
        t = unreal.AssetImportTask()
        t.filename = f
        t.destination_path = dest
        t.automated = True
        t.replace_existing = True
        t.save = False
        tasks.append(t)
    tools.import_asset_tasks(tasks)
    out = []
    for t in tasks:
        out.extend(t.imported_object_paths)
    return out


# ---------------------------------------------------------------------------------------------
# Textures
# ---------------------------------------------------------------------------------------------


def import_textures():
    files = sorted(os.path.join(TEX_DIR, f) for f in os.listdir(TEX_DIR) if f.endswith(".png"))
    import_files(files, TEX_PATH)
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        tex = unreal.load_asset(f"{TEX_PATH}/{name}")
        if not tex:
            log(f"missing texture {name}")
            continue
        if name.endswith("_N"):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
            tex.set_editor_property("srgb", False)
        elif name.endswith("_ORM"):
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
            tex.set_editor_property("srgb", False)
        else:
            tex.set_editor_property("srgb", True)
        lib.save_loaded_asset(tex)
    log(f"{len(files)} textures")


def texture(name):
    return unreal.load_asset(f"{TEX_PATH}/T_{name}")


# ---------------------------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------------------------


def new_asset(name, path, cls, factory):
    """Reuse an existing asset (keeps references intact), otherwise create it."""
    full = f"{path}/{name}"
    if lib.does_asset_exist(full):
        asset = unreal.load_asset(full)
        if isinstance(asset, unreal.Material):
            mel.delete_all_material_expressions(asset)
        return asset
    return tools.create_asset(name, path, cls, factory)


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def world_uvs(mat):
    """UVs from the world position (metres): floors use XY, walls XZ or YZ depending on the normal."""
    wp = expr(mat, unreal.MaterialExpressionWorldPosition, -2200, 0)
    floor = expr(mat, unreal.MaterialExpressionComponentMask, -2000, -150, r=True, g=True, b=False, a=False)
    wall_x = expr(mat, unreal.MaterialExpressionComponentMask, -2000, 0, r=True, g=False, b=True, a=False)
    wall_y = expr(mat, unreal.MaterialExpressionComponentMask, -2000, 150, r=False, g=True, b=True, a=False)
    for m in (floor, wall_x, wall_y):
        mel.connect_material_expressions(wp, "", m, "")
    normal = expr(mat, unreal.MaterialExpressionVertexNormalWS, -2200, 350)
    absn = expr(mat, unreal.MaterialExpressionAbs, -2050, 350)
    mel.connect_material_expressions(normal, "", absn, "")
    ny = expr(mat, unreal.MaterialExpressionComponentMask, -1900, 300, r=False, g=True, b=False, a=False)
    nz = expr(mat, unreal.MaterialExpressionComponentMask, -1900, 420, r=False, g=False, b=True, a=False)
    mel.connect_material_expressions(absn, "", ny, "")
    mel.connect_material_expressions(absn, "", nz, "")
    wall = expr(mat, unreal.MaterialExpressionLinearInterpolate, -1800, 80)
    mel.connect_material_expressions(wall_y, "", wall, "A")
    mel.connect_material_expressions(wall_x, "", wall, "B")
    mel.connect_material_expressions(ny, "", wall, "Alpha")
    uv = expr(mat, unreal.MaterialExpressionLinearInterpolate, -1650, 0)
    mel.connect_material_expressions(wall, "", uv, "A")
    mel.connect_material_expressions(floor, "", uv, "B")
    mel.connect_material_expressions(nz, "", uv, "Alpha")
    metres = expr(mat, unreal.MaterialExpressionMultiply, -1500, 0, const_b=0.01)
    mel.connect_material_expressions(uv, "", metres, "A")
    return metres


def build_surface_master(name="M_ItemSurface", world_aligned=False):
    """Textured PBR surface: base colour x tint, roughness and metallic from ORM, normal map, optional glow.

    world_aligned: UVs from the world position (room floors and walls built from scaled cubes).
    """
    mat = new_asset(name, MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    # Nanite meshes otherwise draw the default material in the game ("missing usage flag Nanite").
    try:
        mat.set_editor_property("used_with_nanite", True)
    except Exception as error:  # noqa: BLE001 – report and keep going
        log(f"{name}: could not set used_with_nanite ({error})")
    uv = world_uvs(mat) if world_aligned else expr(mat, unreal.MaterialExpressionTextureCoordinate, -1400, 0)
    tiling = expr(mat, unreal.MaterialExpressionScalarParameter, -1400, 150, parameter_name="Tiling", default_value=1.0)
    uvs = expr(mat, unreal.MaterialExpressionMultiply, -1200, 50)
    mel.connect_material_expressions(uv, "", uvs, "A")
    mel.connect_material_expressions(tiling, "", uvs, "B")

    bc = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, -300, parameter_name="BaseColorTex",
              texture=texture("Fine_BC"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    orm = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0, parameter_name="ORMTex",
               texture=texture("Fine_ORM"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    nrm = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -900, 300, parameter_name="NormalTex",
               texture=texture("Fine_N"), sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    for t in (bc, orm, nrm):
        mel.connect_material_expressions(uvs, "", t, "UVs")

    tint = expr(mat, unreal.MaterialExpressionVectorParameter, -600, -450, parameter_name="Tint",
                default_value=unreal.LinearColor(1, 1, 1, 1))
    base = expr(mat, unreal.MaterialExpressionMultiply, -400, -300)
    mel.connect_material_expressions(bc, "RGB", base, "A")
    mel.connect_material_expressions(tint, "", base, "B")
    mel.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)

    # Glow (lamp shades, bulbs): base colour x strength, 0 = off.
    glow_strength = expr(mat, unreal.MaterialExpressionScalarParameter, -400, -500, parameter_name="EmissiveStrength", default_value=0.0)
    glow = expr(mat, unreal.MaterialExpressionMultiply, -200, -400)
    mel.connect_material_expressions(base, "", glow, "A")
    mel.connect_material_expressions(glow_strength, "", glow, "B")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

    rough_scale = expr(mat, unreal.MaterialExpressionScalarParameter, -600, 80, parameter_name="RoughnessScale", default_value=1.0)
    rough = expr(mat, unreal.MaterialExpressionMultiply, -400, 0)
    mel.connect_material_expressions(orm, "G", rough, "A")
    mel.connect_material_expressions(rough_scale, "", rough, "B")
    rough_sat = expr(mat, unreal.MaterialExpressionSaturate, -250, 0)
    mel.connect_material_expressions(rough, "", rough_sat, "")
    mel.connect_material_property(rough_sat, "", unreal.MaterialProperty.MP_ROUGHNESS)

    metal_val = expr(mat, unreal.MaterialExpressionScalarParameter, -600, 180, parameter_name="Metallic", default_value=0.0)
    metal = expr(mat, unreal.MaterialExpressionMultiply, -400, 150)
    mel.connect_material_expressions(orm, "B", metal, "A")
    mel.connect_material_expressions(metal_val, "", metal, "B")
    mel.connect_material_property(metal, "", unreal.MaterialProperty.MP_METALLIC)
    mel.connect_material_property(orm, "R", unreal.MaterialProperty.MP_AMBIENT_OCCLUSION)
    mel.connect_material_property(nrm, "RGB", unreal.MaterialProperty.MP_NORMAL)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def build_glass_master():
    mat = new_asset("M_ItemGlass", MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    modes = [m for m in dir(unreal.TranslucencyLightingMode) if "FORWARD" in m.upper()]
    log(f"translucency modes with forward: {modes}")
    if modes:
        mat.set_editor_property("translucency_lighting_mode", getattr(unreal.TranslucencyLightingMode, modes[0]))
    mat.set_editor_property("two_sided", True)
    tint = expr(mat, unreal.MaterialExpressionVectorParameter, -500, -200, parameter_name="Tint",
                default_value=unreal.LinearColor(0.7, 0.8, 0.85, 1))
    rough = expr(mat, unreal.MaterialExpressionScalarParameter, -500, 0, parameter_name="Roughness", default_value=0.05)
    opacity = expr(mat, unreal.MaterialExpressionScalarParameter, -500, 150, parameter_name="Opacity", default_value=0.18)
    mel.connect_material_property(tint, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    spec = expr(mat, unreal.MaterialExpressionConstant, -500, 300, r=1.0)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def build_plan_mark_master():
    """Footprints and rings of the plan on the floor: unlit, see-through, colour and opacity per instance.

    Per-instance custom data (UInstancedStaticMeshComponent, 4 floats): 0..2 colour (linear), 3 opacity.
    Glow scales the colour, because the scene runs with a fixed dark exposure at night.
    """
    mat = new_asset("M_PlanMark", MAT_PATH, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    mat.set_editor_property("two_sided", True)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    color = expr(mat, unreal.MaterialExpressionPerInstanceCustomData3Vector, -600, -150, data_index=0,
                 const_default_value=unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    alpha = expr(mat, unreal.MaterialExpressionPerInstanceCustomData, -600, 100, data_index=3, const_default_value=1.0)
    glow = expr(mat, unreal.MaterialExpressionScalarParameter, -600, -20, parameter_name="Glow", default_value=2.0)
    lit = expr(mat, unreal.MaterialExpressionMultiply, -300, -100)
    mel.connect_material_expressions(color, "", lit, "A")
    mel.connect_material_expressions(glow, "", lit, "B")
    mel.connect_material_property(lit, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(alpha, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def build_instances(surface, glass, room):
    out = {}
    for name, spec in dc_materials.MATERIALS.items():
        mi = new_asset(f"MI_Item_{name}", MAT_PATH, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
        tint = dc_materials.srgb_to_linear(spec["tint"])
        color = unreal.LinearColor(tint[0], tint[1], tint[2], 1.0)
        if spec.get("glass"):
            mel.set_material_instance_parent(mi, glass)
            mel.set_material_instance_vector_parameter_value(mi, "Tint", color)
            mel.set_material_instance_scalar_parameter_value(mi, "Roughness", spec["rough"])
        else:
            mel.set_material_instance_parent(mi, room if spec.get("room") else surface)
            tex = spec["tex"]
            mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", texture(f"{tex}_BC"))
            mel.set_material_instance_texture_parameter_value(mi, "ORMTex", texture(f"{tex}_ORM"))
            mel.set_material_instance_texture_parameter_value(mi, "NormalTex", texture(f"{tex}_N"))
            mel.set_material_instance_vector_parameter_value(mi, "Tint", color)
            mel.set_material_instance_scalar_parameter_value(mi, "Tiling", spec["tiling"])
            mel.set_material_instance_scalar_parameter_value(mi, "RoughnessScale", spec["rough"])
            mel.set_material_instance_scalar_parameter_value(mi, "Metallic", spec["metal"])
            mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", spec.get("emissive", 0.0))
        lib.save_loaded_asset(mi)
        out[name] = mi
    log(f"{len(out)} material instances")
    return out


# ---------------------------------------------------------------------------------------------
# Meshes
# ---------------------------------------------------------------------------------------------


def import_meshes(instances, only=None):
    files = sorted(os.path.join(EXPORT_DIR, f) for f in os.listdir(EXPORT_DIR) if f.endswith(".fbx"))
    if only:
        files = [f for f in files if os.path.splitext(os.path.basename(f))[0][3:] in only]
    imported = import_files(files, MESH_PATH)
    log(f"imported {imported}")
    # The editor subsystem is missing in commandlets; the older library does the same.
    sm_sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem) or unreal.EditorStaticMeshLibrary
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        mesh = unreal.load_asset(f"{MESH_PATH}/{name}")
        if not isinstance(mesh, unreal.StaticMesh):
            log(f"no static mesh for {name}")
            continue
        materials = mesh.get_editor_property("static_materials")
        for i, slot in enumerate(materials):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            key = slot_name[3:] if slot_name.startswith("MI_") else slot_name
            mi = instances.get(key)
            if mi:
                mesh.set_material(i, mi)
            else:
                log(f"{name}: unknown slot {slot_name}")
        sm_sub.remove_collisions(mesh)
        if name.startswith("SM_Plan_"):
            # Plan marks: own material, nothing to bump into.
            plan_mark = unreal.load_asset(f"{MAT_PATH}/M_PlanMark")
            for i in range(len(materials)):
                mesh.set_material(i, plan_mark)
        else:
            sm_sub.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
        box = mesh.get_bounding_box()
        log(f"{name}: slots {[str(m.get_editor_property('material_slot_name')) for m in materials]} bounds {box.min} .. {box.max}")
        lib.save_loaded_asset(mesh)
    # Interchange may have created its own materials/textures next to the meshes – not used.
    for path in lib.list_assets(MESH_PATH, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        if asset and not isinstance(asset, unreal.StaticMesh):
            log(f"remove unused import byproduct {path}")
            lib.delete_asset(path)


def main():
    args = [a for a in sys.argv[1:]]
    only = None
    for a in args:
        if a.startswith("items="):
            only = a[6:].split(",")
    import_textures()
    surface = build_surface_master()
    glass = build_glass_master()
    room = build_surface_master("M_RoomSurface", world_aligned=True)
    instances = build_instances(surface, glass, room)
    build_plan_mark_master()
    if os.environ.get("DC_SKIP_MESHES") != "1":  # set to only refresh textures and materials
        import_meshes(instances, only)
    log("done")


main()
