"""Unreal side of the character pipeline (run as commandlet, editor closed, absolute script path):

  UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript -script="C:/.../Art/Characters/ue_import_character.py Rosa" -unattended

Imports Art/Characters/<Name>/Export/SK_<Name>.fbx (skeletal mesh, skeleton, physics asset, all actions) and
T_<Name>_BC.png into /Game/DerClou/Characters/<Name>:

  SK_<Name>, SK_<Name>_Skeleton, SK_<Name>_PhysicsAsset, A_<Name>_Idle/Walk/Jog/Work/Pickup, T_<Name>_BC, MI_<Name>

plus the shared master material /Game/DerClou/Characters/M_Character (base colour texture, roughness, specular,
used with skeletal meshes). The game finds the model by these names (DCVisuals::CrewModel).
"""

import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
BASE = "/Game/DerClou/Characters"
ACTIONS = ("Idle", "Walk", "Jog", "Work", "Pickup")

tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary


def log(msg):
    unreal.log_warning(f"[character] {msg}")


def import_file(path, dest, options=None):
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = dest
    t.automated = True
    t.replace_existing = True
    t.save = False
    if options is not None:
        t.options = options
    tools.import_asset_tasks([t])
    return list(t.imported_object_paths)


def expr(mat, cls, x, y, **props):
    e = mel.create_material_expression(mat, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def build_master(tex):
    """Base colour from the baked texture, plain roughness and specular; usable on skeletal meshes."""
    path = f"{BASE}/M_Character"
    if lib.does_asset_exist(path):
        mat = unreal.load_asset(path)
        mel.delete_all_material_expressions(mat)
    else:
        mat = tools.create_asset("M_Character", BASE, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_skeletal_mesh", True)
    bc = expr(mat, unreal.MaterialExpressionTextureSampleParameter2D, -700, -150, parameter_name="BaseColorTex",
              texture=tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    mel.connect_material_property(bc, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = expr(mat, unreal.MaterialExpressionScalarParameter, -700, 100, parameter_name="Roughness", default_value=0.78)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = expr(mat, unreal.MaterialExpressionScalarParameter, -700, 220, parameter_name="Specular", default_value=0.3)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    mel.recompile_material(mat)
    lib.save_loaded_asset(mat)
    return mat


def fbx_options():
    """Legacy FBX options; the Interchange importer converts them where it is active."""
    o = unreal.FbxImportUI()
    o.set_editor_property("import_mesh", True)
    o.set_editor_property("import_as_skeletal", True)
    o.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_SKELETAL_MESH)
    o.set_editor_property("import_animations", True)
    o.set_editor_property("import_materials", False)
    o.set_editor_property("import_textures", False)
    o.set_editor_property("create_physics_asset", True)
    o.skeletal_mesh_import_data.set_editor_property("import_morph_targets", False)
    return o


def main():
    name = next((a for a in sys.argv[1:] if not a.startswith("-")), "Rosa")
    dest = f"{BASE}/{name}"
    src = os.path.join(HERE, name, "Export")
    fbx = os.path.join(src, f"SK_{name}.fbx")
    png = os.path.join(src, f"T_{name}_BC.png")

    import_file(png, dest)
    tex = unreal.load_asset(f"{dest}/T_{name}_BC")
    tex.set_editor_property("srgb", True)
    lib.save_loaded_asset(tex)
    master = build_master(tex)
    mi_path = f"{dest}/MI_{name}"
    mi = unreal.load_asset(mi_path) if lib.does_asset_exist(mi_path) else tools.create_asset(
        f"MI_{name}", dest, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", tex)
    lib.save_loaded_asset(mi)

    imported = import_file(fbx, dest, fbx_options())
    log(f"imported: {imported}")
    mesh = unreal.load_asset(f"{dest}/SK_{name}")
    if not isinstance(mesh, unreal.SkeletalMesh):
        log(f"no skeletal mesh SK_{name} - assets now: {lib.list_assets(dest, recursive=True, include_folder=False)}")
        return
    skeleton = mesh.get_editor_property("skeleton")
    # Elements of the returned array are copies: build new slots instead of editing them.
    mats = [unreal.SkeletalMaterial(material_interface=mi, material_slot_name=m.get_editor_property("material_slot_name"))
            for m in mesh.get_editor_property("materials")]
    mesh.set_editor_property("materials", mats)
    log(f"slots: {[(str(m.get_editor_property('material_slot_name')), m.get_editor_property('material_interface').get_name()) for m in mesh.get_editor_property('materials')]}")
    box = mesh.get_bounds()
    log(f"SK_{name}: skeleton {skeleton.get_name() if skeleton else None}, {len(mats)} material slot(s), "
        f"bounds origin {box.origin} extent {box.box_extent}")
    lib.save_loaded_asset(mesh)

    # Animations: whatever the importer called them, rename to A_<Name>_<Action>; delete byproducts.
    for path in lib.list_assets(dest, recursive=True, include_folder=False):
        asset = unreal.load_asset(path)
        short = path.split("/")[-1].split(".")[0]
        if isinstance(asset, unreal.AnimSequence):
            action = next((a for a in ACTIONS if short.endswith(f"{name}_{a}") or short.endswith(f"_{a}")), None)
            target = f"{dest}/A_{name}_{action}" if action else None
            if target and short != f"A_{name}_{action}":
                if lib.does_asset_exist(target):
                    lib.delete_asset(target)
                lib.rename_asset(path, target)
                asset = unreal.load_asset(target)
            if asset:
                asset.set_editor_property("enable_root_motion", False)
                lib.save_loaded_asset(asset)
                log(f"anim {asset.get_name()}: {asset.get_play_length():.2f} s")
        elif isinstance(asset, (unreal.Material, unreal.MaterialInstance)) and short not in (f"MI_{name}",):
            log(f"remove import byproduct {path}")
            lib.delete_asset(path)
        elif isinstance(asset, unreal.Texture) and short != f"T_{name}_BC":
            log(f"remove import byproduct {path}")
            lib.delete_asset(path)
    # Skeleton (and whatever else the importer made) is only in memory so far.
    lib.save_directory(dest, only_if_is_dirty=False, recursive=True)
    log(f"assets: {sorted(p.split('/')[-1].split('.')[0] for p in lib.list_assets(dest, recursive=True, include_folder=False))}")
    log("done")


main()
