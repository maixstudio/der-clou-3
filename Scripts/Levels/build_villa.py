"""Builds L_Villa01: Villa Lindenhof, first section (plot, Remise, ground floor, cellar) from villa_layout.py.

  UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript -script="C:/Dev/der-clou-3/Scripts/Levels/build_villa.py"

Run with the editor closed and an absolute script path; delete Content/DerClou/Maps/L_Villa01.umap before
(the commandlet cannot always delete a map it did not load). Starts from a copy of L_House01 (sky, moon, post process,
crew, neighbour, nav mesh actors), removes that house and builds the villa from the same data as the floor plan
drawing (Docs/Villa/Villa-Grundriss.html). The 1st floor, windows you can climb through and motion-sensor lights
come in later steps; their places are already in the data.

Axes: plan metres (x east, y north) -> Unreal cm with X = y * 100, Y = x * 100.
Actors that belong to a storey of the house carry the tag DCFloor_EG or DCFloor_KG: the camera shows one storey
and hides everything above it (UDCFloorSubsystem).
"""

import math
import os
import sys
from collections import defaultdict

import unreal

HERE = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Scripts", "Levels")
sys.path.insert(0, HERE)
import villa_layout as V  # noqa: E402
import villa_plan as P  # noqa: E402

SOURCE = "/Game/DerClou/Maps/L_House01"
TARGET = "/Game/DerClou/Maps/L_Villa01"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"
CONE = "/Engine/BasicShapes/Cone.Cone"
ITEM_MAT = "/Game/DerClou/Items/Materials/"
ITEM_TEX = "/Game/DerClou/Items/Textures/T_"
VILLA_MAT = "/Game/DerClou/Villa/Materials"
DECO = "/Game/DerClou/Items/Meshes/SM_Deco_"

FLOOR_Z = {"EG": 0.0, "KG": -290.0}
WALL_HEIGHT = {"EG": 250.0, "KG": 265.0}  # KG walls reach the underside of the ground-floor slab (-25)
SLAB = 25.0
# Gates as tall as doors: the crew capsule (about 1.85 m) bumps into a lower top frame.
DOOR_HEIGHT = {"haustuer": 220.0, "tor": 220.0, "schiebetor": 210.0, "pforte": 210.0, "offen": 220.0}
WINDOW_SPAN = {"fenster": (90.0, 210.0), "kipp": (140.0, 210.0), "milch": (140.0, 210.0), "bodentief": (0.0, 210.0),
               "keller": (180.0, 250.0)}
TAG = {"EG": "DCFloor_EG", "KG": "DCFloor_KG"}

REMOVE_PREFIXES = ("Wand_", "Sturz_", "Boden_Haus", "Tuer_", "Raum_", "Item_", "Deko_", "Licht_Schreibtischlampe",
                   "Licht_Stehlampe", "Licht_Flurlampe", "Floor")
FOLDERS = ("Villa",)

# Material instances made for the villa: name -> (parent, texture, tint, tiling, roughness, metal, opacity)
VILLA_MATERIALS = {
    "Grass": ("M_RoomSurface", "Fabric", (0.26, 0.40, 0.18), 1.6, 1.0, 0.0, None),
    "Gravel": ("M_RoomSurface", "Concrete", (0.78, 0.72, 0.60), 1.2, 1.0, 0.0, None),
    "Paving": ("M_RoomSurface", "Concrete", (0.62, 0.62, 0.60), 0.6, 1.0, 0.0, None),
    "Asphalt": ("M_RoomSurface", "Concrete", (0.20, 0.21, 0.23), 0.4, 1.0, 0.0, None),
    "Stone": ("M_RoomSurface", "Marble", (0.72, 0.70, 0.66), 0.5, 1.0, 0.0, None),
    "Tiles": ("M_RoomSurface", "Marble", (0.92, 0.92, 0.90), 1.4, 0.8, 0.0, None),
    "Plaster": ("M_RoomSurface", "Paint", (0.88, 0.85, 0.78), 1.0, 1.2, 0.0, None),
    "CellarWall": ("M_RoomSurface", "Paint", (0.74, 0.74, 0.72), 1.0, 1.4, 0.0, None),
    "Deck": ("M_RoomSurface", "FloorPlanks", (0.85, 0.68, 0.50), 0.5, 1.2, 0.0, None),
    "Hedge": ("M_RoomSurface", "Fabric", (0.12, 0.26, 0.10), 2.5, 1.0, 0.0, None),
    "Soil": ("M_RoomSurface", "Fabric", (0.24, 0.17, 0.11), 2.0, 1.0, 0.0, None),
    "PoolTiles": ("M_RoomSurface", "Marble", (0.50, 0.78, 0.88), 2.0, 0.6, 0.0, None),
    "Leaves": ("M_ItemSurface", "Fabric", (0.20, 0.36, 0.14), 6.0, 1.0, 0.0, None),
    "Water": ("M_ItemGlass", None, (0.20, 0.55, 0.70), 1.0, 0.05, 0.0, 0.45),
}

ROOM_FLOOR = {"wohnen": "MI_Item_Floor_Planks", "schlafen": "MI_Item_Floor_Planks", "nass": "Tiles", "service": "Tiles",
              "verkehr": "Stone", "technik": "MI_Item_Ground_Concrete", "wert": "Stone", "garage": "MI_Item_Ground_Concrete"}

# Facing of furniture (model front = local +Y) and of paintings (canvas = local -X), as Unreal yaw.
FACE = {"E": 0.0, "W": 180.0, "N": -90.0, "S": 90.0}
PAINTING_FACE = {"E": -90.0, "W": 90.0, "N": 180.0, "S": 0.0}
# Light aim (plan direction -> Unreal yaw).
AIM = {"N": 0.0, "E": 90.0, "S": 180.0, "W": -90.0}

# Pickups: label, item, plan (x, y), facing ("E".. or yaw number), z cm above the floor, floor tag, locked, contents.
THINGS = [
    # Erdgeschoss
    ("Item_Anrichte", "dresser", (29.33, 25.6), "E", 0, "EG", False, [("figurine", "", False), ("figurine", "", False)]),
    ("Item_Esstisch", "dining_table", (32.7, 26.2), 90.0, 0, "EG", False, []),
    ("Item_Stuhl_1", "chair", (32.2, 25.3), "N", 0, "EG", False, []),
    ("Item_Stuhl_2", "chair", (33.2, 25.3), "N", 0, "EG", False, []),
    ("Item_Stuhl_3", "chair", (32.2, 27.1), "S", 0, "EG", False, []),
    ("Item_Stuhl_4", "chair", (33.2, 27.1), "S", 0, "EG", False, []),
    ("Item_Gemaelde_Halle", "painting", (34.1, 16.21), "N@", 0, "EG", False, []),
    ("Item_Sofa", "sofa", (41.6, 25.5), "E", 0, "EG", False, [("cash", "", False)]),
    ("Item_Sessel_Wohnzimmer", "armchair", (42.6, 27.7), "S", 0, "EG", False, []),
    ("Item_Fluegel", "piano", (42.8, 29.5), "S", 0, "EG", False, []),
    ("Item_Bronzefigur", "statue", (37.0, 24.6), 0.0, 0, "EG", False, []),
    ("Item_Gemaelde_Wohnzimmer", "painting", (42.5, 21.12), "N@", 0, "EG", False, []),
    ("Item_Vitrine_Bibliothek", "display_case", (36.36, 18.5), "E", 0, "EG", True, [("brooch", "", False), ("figurine", "", False)]),
    ("Item_Lesetisch", "desk", (39.3, 17.35), "N", 0, "EG", False, [("desk_drawer", "", True), ("files", "desk_drawer", False),
                                                                   ("laptop", "desk_drawer", False)]),
    ("Item_Sessel_Bibliothek", "armchair", (40.9, 19.4), "W", 0, "EG", False, []),
    ("Item_Gemaelde_Geheim_1", "painting", (43.0, 16.21), "N@", 0, "EG", False, []),
    ("Item_Gemaelde_Geheim_2", "painting", (43.79, 17.5), "W@", 0, "EG", False, []),
    ("Item_Garderobenschrank", "wardrobe", (25.87, 20.25), "E", 0, "EG", False,
     [("coat", "", False), ("wallet", "coat", False), ("cash", "coat/wallet", False)]),
    ("Item_Vorratskiste", "cardboard_box", (22.4, 25.2), "E", 0, "EG", False, [("cash", "", False)]),
    ("Item_Waeschekiste", "cardboard_box", (22.4, 17.2), "E", 0, "EG", False, [("flashlight", "", False)]),
    # Keller
    ("Item_Tresor", "safe", (43.5, 17.3), "W", 0, "KG", True,
     [("cash", "", False), ("cash", "", False), ("watch", "", False), ("necklace", "", False), ("ring", "", False)]),
    ("Item_Vitrine_Tresorraum", "display_case", (38.2, 16.42), "N", 0, "KG", True, [("brooch", "", False), ("ring", "", False)]),
    ("Item_Bronzefigur_Tresorraum", "statue", (43.2, 21.3), 0.0, 0, "KG", False, []),
    ("Item_Weinkiste_1", "cardboard_box", (31.6, 21.45), 90.0, 0, "KG", False, [("cash", "", False)]),
    ("Item_Weinkiste_2", "cardboard_box", (32.4, 21.45), 90.0, 0, "KG", False, []),
    ("Item_Weinkiste_3", "cardboard_box", (36.3, 21.45), 90.0, 0, "KG", False, []),
    ("Item_Lagerkiste_1", "cardboard_box", (26.5, 16.45), 90.0, 0, "KG", False, [("flashlight", "", False)]),
    ("Item_Lagerkiste_2", "cardboard_box", (28.3, 16.45), 90.0, 0, "KG", False, []),
    ("Item_Sofa_Hobby", "sofa", (33.5, 29.4), "S", 0, "KG", False, []),
    ("Item_Bar", "dresser", (36.68, 26.5), "W", 0, "KG", False, [("cash", "", False)]),
    ("Item_Sessel_Hobby", "armchair", (30.2, 29.2), "S", 0, "KG", False, []),
    ("Item_Alter_Schrank", "wardrobe", (37.37, 27.0), "E", 0, "KG", False, [("coat", "", False)]),
    ("Item_Altes_Gemaelde", "painting", (39.5, 29.79), "S@", 0, "KG", False, []),
    ("Item_Abstellkiste", "cardboard_box", (39.4, 25.0), "E", 0, "KG", False, []),
    # Remise (on the workbench, 90 cm)
    ("Item_Brecheisen", "crowbar", (11.8, 21.9), 90.0, 91, None, False, []),
    ("Item_Werkzeugweste", "tool_vest", (13.0, 21.9), 90.0, 91, None, False, []),
    ("Item_Taschenlampe", "flashlight", (10.8, 21.9), 0.0, 91, None, False, []),
    ("Item_Geraetekiste", "cardboard_box", (14.3, 24.4), "W", 0, None, False, []),
    # Street: what the crew brought
    ("Item_Rucksack", "backpack", (12.4, -1.2), 0.0, 0, None, False, []),
]

# Fixed furniture without interaction (blocks walking): label, floor, plan rect, z from/to above the floor, material.
DECO_BOXES = [
    ("Deko_Kochinsel", "EG", (25.0, 26.4, 27.4, 27.4), 0, 92, "MI_Item_Marble"),
    ("Deko_Kuechenzeile_Nord", "EG", (25.0, 29.25, 26.9, 29.85), 0, 90, "MI_Item_Wood_Oak"),
    ("Deko_Kuechenzeile_West", "EG", (22.15, 27.0, 22.75, 29.85), 0, 90, "MI_Item_Wood_Oak"),
    ("Deko_Kamin", "EG", (43.4, 24.6, 43.85, 26.4), 0, 110, "MI_Item_Marble"),
    ("Deko_Regal_West_1", "EG", (36.08, 16.2, 36.45, 17.9), 0, 220, "MI_Item_Wood_Walnut"),
    ("Deko_Regal_West_2", "EG", (36.08, 19.1, 36.45, 20.9), 0, 220, "MI_Item_Wood_Walnut"),
    ("Deko_Regal_Nord", "EG", (36.8, 20.55, 38.7, 20.92), 0, 220, "MI_Item_Wood_Walnut"),
    ("Deko_Regal_Ost", "EG", (43.48, 18.6, 43.85, 20.9), 0, 220, "MI_Item_Wood_Walnut"),
    ("Deko_Regal_Geheimtuer", "EG", (41.55, 16.2, 41.92, 16.8), 0, 220, "MI_Item_Wood_Walnut"),
    ("Deko_WC", "EG", (28.2, 16.2, 28.85, 16.75), 0, 45, "MI_Item_Porcelain"),
    ("Deko_Waschbecken", "EG", (26.0, 16.2, 26.6, 16.6), 70, 88, "MI_Item_Porcelain"),
    ("Deko_Waschmaschine", "EG", (24.6, 16.2, 25.35, 16.85), 0, 85, "MI_Item_Porcelain"),
    ("Deko_Heizung", "KG", (22.2, 16.2, 23.4, 17.4), 0, 180, "MI_Item_Steel"),
    ("Deko_Sicherungskasten", "KG", (22.17, 20.0, 22.35, 20.8), 120, 200, "MI_Item_Steel"),
    ("Deko_Alarmzentrale", "KG", (24.4, 21.72, 25.6, 21.9), 120, 180, "MI_Item_Paint_Red"),
    ("Deko_Weinregal_Sued", "KG", (31.0, 16.2, 36.8, 16.7), 0, 200, "MI_Item_Wood_Oak"),
    ("Deko_Weinregal_Mitte", "KG", (31.0, 18.3, 36.8, 18.8), 0, 180, "MI_Item_Wood_Oak"),
    ("Deko_Billard", "KG", (29.7, 27.2, 32.5, 28.8), 0, 80, "MI_Item_Fabric_Olive"),
    ("Deko_Laufband", "KG", (22.6, 24.6, 24.4, 25.4), 0, 40, "MI_Item_Plastic_Black"),
    ("Deko_Hantelbank", "KG", (23.6, 28.2, 25.0, 28.6), 0, 50, "MI_Item_Leather_Black"),
    ("Deko_Saunabank", "KG", (26.65, 29.3, 28.85, 29.85), 0, 45, "MI_Item_Wood_Oak"),
    ("Deko_Werkbank", None, (10.3, 21.6, 13.4, 22.2), 0, 90, "MI_Item_Wood_Oak"),
    ("Deko_Regal_Geraeteraum", None, (10.2, 24.5, 13.0, 24.85), 0, 180, "MI_Item_Wood_Oak"),
    ("Deko_Auto", None, (3.45, 18.6, 5.25, 22.9), 25, 120, "MI_Item_Lacquer_Black"),
    ("Deko_Auto_Dach", None, (3.6, 19.6, 5.1, 21.8), 120, 160, "MI_Item_Screen_Black"),
    ("Deko_Oldtimer", None, (7.65, 18.6, 9.55, 22.9), 25, 112, "MI_Item_Paint_Red"),
    ("Deko_Oldtimer_Dach", None, (7.8, 19.7, 9.4, 21.6), 112, 150, "MI_Item_Ivory"),
]

# Indoor lamps: label, floor tag (None = Remise), plan (x, y), height above the floor (cm), lumens, radius (cm).
INDOOR_LIGHTS = [
    ("Licht_Halle", "EG", (32.5, 20.0), 235, 550, 750),
    ("Licht_Kueche", "EG", (25.6, 27.0), 235, 380, 650),
    ("Licht_Esszimmer", "EG", (32.5, 27.0), 235, 300, 600),
    ("Licht_Wohnzimmer", "EG", (37.2, 29.3), 150, 500, 700),
    ("Licht_Bibliothek", "EG", (39.3, 17.4), 115, 320, 500),
    ("Licht_Flur", "EG", (25.5, 23.0), 235, 200, 450),
    ("Licht_Kellerflur_West", "KG", (26.0, 23.0), 230, 260, 650),
    ("Licht_Kellerflur_Ost", "KG", (38.0, 23.0), 230, 260, 650),
    ("Licht_Tresorraum", "KG", (40.5, 19.0), 230, 260, 600),
    ("Licht_Weinkeller", "KG", (34.0, 19.6), 230, 180, 550),
    ("Licht_Hobbyraum", "KG", (33.0, 27.0), 230, 300, 650),
    ("Licht_Technik", "KG", (24.0, 19.0), 230, 150, 450),
    ("Licht_Werkstatt", None, (12.5, 20.2), 235, 300, 550),
]

# Motion-sensor spots aim away from the wall they hang on.
BM_AIM = {"Seiteneingang": "W", "Garage": "S", "Kelleraußentreppe": "N"}

CREW = {"Crew_Vic": (13.6, -1.1), "Crew_Rosa": (15.0, -1.1)}
NEIGHBOUR = (67.0, 21.0)


def log(msg):
    unreal.log_warning(f"[villa] {msg}")


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as err:  # property names differ between engine versions
        log(f"  {type(obj).__name__}.{prop}: {err}")
        return False


def actor_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def ue(x, y, z=0.0):
    """Plan metres -> Unreal location (cm)."""
    return unreal.Vector(y * 100.0, x * 100.0, z)


def spawn(cls, location, yaw=0.0, pitch=0.0, roll=0.0):
    return actor_sub().spawn_actor_from_class(cls, location, unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))


_assets = {}


def asset(path):
    if path not in _assets:
        _assets[path] = unreal.load_asset(path)
    return _assets[path]


def material(name):
    if name.startswith("MI_Item_"):
        return asset(ITEM_MAT + name)
    return asset(f"{VILLA_MAT}/MI_Villa_{name}")


def finish(a, label, folder, tag=None, collision=True, hidden=False, shadow=True):
    a.set_actor_label(label)
    a.set_folder_path(f"Villa/{folder}")
    if tag:
        try_set(a, "tags", [unreal.Name(tag)])
    smc = a.get_component_by_class(unreal.StaticMeshComponent)
    if smc:
        if not collision:
            # The profile is what gets saved with the level (set_collision_enabled alone did not stick).
            smc.set_collision_profile_name("NoCollision")
            smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            try_set(smc, "can_ever_affect_navigation", False)
        if not shadow:
            try_set(smc, "cast_shadow", False)
    if hidden:
        a.set_actor_hidden_in_game(True)
    return a


def mesh_actor(mesh, location, scale, mat, yaw=0.0, pitch=0.0):
    a = spawn(unreal.StaticMeshActor, location, yaw, pitch)
    smc = a.static_mesh_component
    smc.set_static_mesh(asset(mesh))
    a.set_actor_scale3d(scale)
    if mat:
        smc.set_material(0, material(mat))
    return a


def box(label, folder, rect, z0, z1, mat, tag=None, collision=True, hidden=False, shadow=True):
    """Axis-aligned box over a plan rectangle (metres), z0..z1 in cm."""
    x0, y0, x1, y1 = rect
    a = mesh_actor(CUBE, ue((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2),
                   unreal.Vector((y1 - y0), (x1 - x0), (z1 - z0) / 100.0), mat)
    return finish(a, label, folder, tag, collision, hidden, shadow)


def disc(label, folder, cx, cy, r, z0, z1, mat, collision=False, mesh=CYLINDER, tag=None):
    a = mesh_actor(mesh, ue(cx, cy, (z0 + z1) / 2), unreal.Vector(r * 2.0, r * 2.0, (z1 - z0) / 100.0), mat)
    return finish(a, label, folder, tag, collision)


def subtract(rect, holes):
    """Splits a rectangle into rectangles that leave out the holes (grid decomposition, merged per row)."""
    x0, y0, x1, y1 = rect
    xs = sorted({x0, x1} | {v for h in holes for v in (h[0], h[2]) if x0 < v < x1})
    ys = sorted({y0, y1} | {v for h in holes for v in (h[1], h[3]) if y0 < v < y1})
    out = []
    for ya, yb in zip(ys, ys[1:]):
        row = None
        for xa, xb in zip(xs, xs[1:]):
            mx, my = (xa + xb) / 2, (ya + yb) / 2
            inside = any(h[0] < mx < h[2] and h[1] < my < h[3] for h in holes)
            if inside:
                if row:
                    out.append(row)
                    row = None
            elif row:
                row = (row[0], ya, xb, yb)
            else:
                row = (xa, ya, xb, yb)
        if row:
            out.append(row)
    # merge vertically where rows line up
    merged = []
    for r in out:
        for i, m in enumerate(merged):
            if abs(m[0] - r[0]) < 1e-6 and abs(m[2] - r[2]) < 1e-6 and abs(m[3] - r[1]) < 1e-6:
                merged[i] = (m[0], m[1], m[2], r[3])
                break
        else:
            merged.append(r)
    return merged


# ---------------------------------------------------------------------------------------------------------
# Materials
# ---------------------------------------------------------------------------------------------------------

def build_materials():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary
    for name, (parent, tex, tint, tiling, rough, metal, opacity) in VILLA_MATERIALS.items():
        path = f"{VILLA_MAT}/MI_Villa_{name}"
        if unreal.EditorAssetLibrary.does_asset_exist(path):
            mi = unreal.load_asset(path)
        else:
            mi = tools.create_asset(f"MI_Villa_{name}", VILLA_MAT, unreal.MaterialInstanceConstant,
                                    unreal.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, asset(ITEM_MAT + parent))
        mel.set_material_instance_vector_parameter_value(mi, "Tint", unreal.LinearColor(tint[0], tint[1], tint[2], 1.0))
        if parent == "M_ItemGlass":
            mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
            mel.set_material_instance_scalar_parameter_value(mi, "Opacity", opacity)
        else:
            mel.set_material_instance_texture_parameter_value(mi, "BaseColorTex", asset(f"{ITEM_TEX}{tex}_BC"))
            mel.set_material_instance_texture_parameter_value(mi, "ORMTex", asset(f"{ITEM_TEX}{tex}_ORM"))
            mel.set_material_instance_texture_parameter_value(mi, "NormalTex", asset(f"{ITEM_TEX}{tex}_N"))
            mel.set_material_instance_scalar_parameter_value(mi, "Tiling", tiling)
            mel.set_material_instance_scalar_parameter_value(mi, "RoughnessScale", rough)
            mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
        unreal.EditorAssetLibrary.save_loaded_asset(mi)
        _assets.pop(path, None)
    log(f"{len(VILLA_MATERIALS)} materials ready")


# ---------------------------------------------------------------------------------------------------------
# House: slabs, floors, walls, windows, doors, stairs, rooms
# ---------------------------------------------------------------------------------------------------------

HOUSE = (21.85, 15.85, 44.15, 30.15)
CELLAR_STAIR_HOLE = (29.15, 17.4, 30.65, 21.85)
OUTSIDE_STAIR_PIT = (44.15, 24.4, 45.65, 30.0)


def build_slabs():
    for i, r in enumerate(subtract(HOUSE, [CELLAR_STAIR_HOLE])):
        box(f"Decke_EG_{i}", "Haus", r, -SLAB, 0.0, "MI_Item_Ground_Concrete", TAG["EG"])
    box("Boden_KG", "Keller", HOUSE, FLOOR_Z["KG"] - 30.0, FLOOR_Z["KG"], "MI_Item_Ground_Concrete", TAG["KG"])
    # Room floor finishes, flat and without collision (the slab carries the walking).
    for floor in ("EG", "KG"):
        rooms = sorted(P.rooms_on(floor), key=lambda r: -(r["rect"][2] - r["rect"][0]) * (r["rect"][3] - r["rect"][1]))
        for i, r in enumerate(rooms):
            mat = ROOM_FLOOR[r["kind"]]
            if floor == "KG" and r["kind"] not in ("wohnen",):
                mat = "MI_Item_Ground_Concrete" if r["kind"] != "wert" else "Stone"
            if r["id"] == "eg_halle":
                mat = "MI_Item_Marble"
            tag = TAG[floor] if r["building"] == "Haus" else None
            folder = "Remise" if r["building"] == "Remise" else ("Keller" if floor == "KG" else "Haus")
            z = FLOOR_Z[floor] + 0.2 + i * 0.05
            if r["id"] == "kg_treppe":
                continue
            # Leave the stair hole open (the hall's floor would otherwise cover the cellar stair).
            pieces = subtract(r["rect"], [CELLAR_STAIR_HOLE]) if floor == "EG" else [r["rect"]]
            for j, rect in enumerate(pieces):
                box(f"Boden_{r['id']}_{j}", folder, rect, z - 0.2, z + 0.8, mat, tag, collision=False)


def wall_material(floor, building, exterior):
    if floor == "KG":
        return "CellarWall"
    if building == "Remise" or exterior:
        return "Plaster"
    return "MI_Item_Wallpaper"


def building_at(floor, x, y):
    for r in P.rooms_on(floor):
        x0, y0, x1, y1 = r["rect"]
        if x0 - 0.5 <= x <= x1 + 0.5 and y0 - 0.5 <= y <= y1 + 0.5:
            return r["building"]
    return "Haus"


def openings_on(floor):
    """(axis, c) -> list of (a, b, z_lo, z_hi, kind, data) relative to the floor."""
    out = defaultdict(list)
    for d in P.doors_on(floor):
        ax, c, p0, p1, _ = P.door_frame(d)
        h = DOOR_HEIGHT.get(d["type"], 210.0)
        out[(ax, round(c, 3))].append((p0, p1, 0.0, h, "door", d))
    for w in V.WINDOWS:
        if w["floor"] != floor:
            continue
        ax = w["axis"]
        c = w["at"][1] if ax == "h" else w["at"][0]
        pos = w["at"][0] if ax == "h" else w["at"][1]
        lo, hi = WINDOW_SPAN[w["type"]]
        out[(ax, round(c, 3))].append((pos - w["width"] / 2, pos + w["width"] / 2, lo, hi, "window", w))
    return out


def wall_box(label, folder, axis, c, a, b, t, z0, z1, mat, tag):
    if axis == "h":
        rect = (a, c - t / 2, b, c + t / 2)
    else:
        rect = (c - t / 2, a, c + t / 2, b)
    return box(label, folder, rect, z0, z1, mat, tag)


def build_walls(floor):
    segs = P.classify_walls(floor)
    opens = openings_on(floor)
    base = FLOOR_Z[floor]
    height = WALL_HEIGHT[floor]
    n = 0
    glass = []
    for axis, c, a, b, t in segs:
        other = "v" if axis == "h" else "h"
        mid_x, mid_y = ((a + b) / 2, c) if axis == "h" else (c, (a + b) / 2)
        building = building_at(floor, mid_x, mid_y)
        tag = TAG[floor] if building == "Haus" else None
        folder = "Remise" if building == "Remise" else ("Keller" if floor == "KG" else "Haus")
        mat = wall_material(floor, building, t > V.INTERIOR_WALL + 1e-6)
        holes = [o for o in opens.get((axis, round(c, 3)), []) if o[1] > a + 1e-6 and o[0] < b - 1e-6]
        cuts = sorted({a, b} | {v for o in holes for v in (o[0], o[1]) if a < v < b})
        for s0, s1 in zip(cuts, cuts[1:]):
            m = (s0 + s1) / 2
            hole = next((o for o in holes if o[0] < m < o[1]), None)
            if hole is None:
                e0 = P.wall_thickness_at(segs, other, s0, c) / 2 if abs(s0 - a) < 1e-6 else 0.0
                e1 = P.wall_thickness_at(segs, other, s1, c) / 2 if abs(s1 - b) < 1e-6 else 0.0
                wall_box(f"Wand_{floor}_{n}", folder, axis, c, s0 - e0, s1 + e1, t, base, base + height, mat, tag)
                n += 1
                continue
            lo, hi = hole[2], hole[3]
            if lo > 0.5:
                wall_box(f"Wand_{floor}_{n}_unten", folder, axis, c, s0, s1, t, base, base + lo, mat, tag)
            if hi < height - 0.5:
                wall_box(f"Wand_{floor}_{n}_oben", folder, axis, c, s0, s1, t, base + hi, base + height, mat, tag)
            n += 1
            if hole[4] == "window":
                glass.append((axis, c, s0, s1, base + lo, base + hi, tag, folder, hole[5]))
    for i, (axis, c, s0, s1, z0, z1, tag, folder, w) in enumerate(glass):
        wall_box(f"Fenster_{floor}_{i}", folder, axis, c, s0, s1, 0.03, z0, z1, "MI_Item_Glass", tag)
        # window frame: thin sill below the glass
        wall_box(f"Fenster_{floor}_{i}_Bank", folder, axis, c, s0, s1, 0.34 if floor == "EG" else 0.32, z0 - 4, z0, "MI_Item_Paint_White", tag)
        if w["type"] == "keller":
            # light well with a grate in front of the cellar window
            ax = w["axis"]
            pos = w["at"][0] if ax == "h" else w["at"][1]
            out_sign = 1.0 if not P.inside_any(P.rooms_on(floor), w["at"][0] + (0.0 if ax == "h" else 0.4),
                                              w["at"][1] + (0.4 if ax == "h" else 0.0)) else -1.0
            if ax == "h":
                r = (pos - w["width"] / 2 - 0.1, c + out_sign * 0.15, pos + w["width"] / 2 + 0.1, c + out_sign * 0.9)
            else:
                r = (c + out_sign * 0.15, pos - w["width"] / 2 - 0.1, c + out_sign * 0.9, pos + w["width"] / 2 + 0.1)
            r = (min(r[0], r[2]), min(r[1], r[3]), max(r[0], r[2]), max(r[1], r[3]))
            box(f"Lichtschacht_{i}", "Aussen", r, 0.0, 2.0, "MI_Item_Iron_Dark", None, collision=False)
    log(f"{floor}: {n} wall pieces, {len(glass)} windows")


LOCK_TEXT = {
    "haustuer": "Sicherheitsschloss mit Mehrfachverriegelung", "tuer": "Zylinderschloss", "glas": "Glastür mit Riegel",
    "glas2": "Glas-Flügeltür mit Riegel", "schiebe": "Hebeschiebetür, innen verriegelt", "stahl": "Stahltür mit Sicherheitszylinder",
    "panzer": "Panzertür mit Zahlenschloss", "gitter": "Gittertür mit Vorhängeschloss", "geheim": "drehbares Bücherregal",
    "tor": "elektrisch – lässt sich nicht knacken", "schiebetor": "elektrisch – lässt sich nicht knacken",
    "pforte": "Gartentor-Schloss", "doppel": "Zimmerschloss",
}
PANEL_COLOR = {
    "haustuer": (0.22, 0.12, 0.06), "tuer": (0.55, 0.36, 0.18), "doppel": (0.62, 0.46, 0.28), "glas": (0.55, 0.75, 0.85),
    "glas2": (0.55, 0.75, 0.85), "schiebe": (0.55, 0.75, 0.85), "stahl": (0.42, 0.44, 0.46), "panzer": (0.25, 0.27, 0.28),
    "gitter": (0.12, 0.12, 0.13), "geheim": (0.30, 0.17, 0.08), "tor": (0.78, 0.78, 0.76), "schiebetor": (0.12, 0.12, 0.13),
    "pforte": (0.12, 0.12, 0.13),
}


def place_door(d, floor_z, tag, folder):
    typ = d["type"]
    if typ == "offen":
        return None
    ax, c, p0, p1, _ = P.door_frame(d)
    width = d["width"]
    if typ == "schiebe":
        # one sliding leaf opens, the other half is fixed glass
        width = d["width"] / 2
        wall_box(f"Festglas_{d['id']}", folder, ax, c, p0 + width, p1, 0.03, floor_z, floor_z + 210.0, "MI_Item_Glass", tag)
    location = ue(p0, c, floor_z) if ax == "h" else ue(c, p0, floor_z)
    yaw = 0.0 if ax == "h" else -90.0
    door = spawn(unreal.DCDoor, location, yaw)
    lock = d.get("lock")
    locked = bool(d.get("locked"))
    try_set(door, "display_name", d["name"])
    try_set(door, "locked", locked)
    try_set(door, "open", False)
    try_set(door, "lock_difficulty", int(lock) if lock is not None else 10)
    try_set(door, "pick_lock_seconds", 20.0 if typ == "panzer" else float((lock or 3) + 1))
    text = LOCK_TEXT.get(typ, "Zimmerschloss")
    if d.get("note"):
        text = f"{text} – {d['note']}"
    try_set(door, "lock_description", text)
    try_set(door, "width", width * 100.0)
    try_set(door, "height", DOOR_HEIGHT.get(typ, 210.0))
    col = PANEL_COLOR.get(typ, (0.5, 0.27, 0.1))
    try_set(door, "panel_color", unreal.LinearColor(col[0], col[1], col[2], 1.0))
    door.set_actor_label(f"Tuer_{d['id']}")
    door.set_folder_path(f"Villa/{folder}")
    if tag:
        try_set(door, "tags", [unreal.Name(tag)])
    return door


def build_doors():
    n = 0
    for d in V.DOORS:
        if d["floor"] not in ("EG", "KG", "Aussen"):
            continue
        if d["floor"] == "Aussen":
            place_door(d, 0.0, None, "Aussen")
        else:
            remise = d["id"].startswith("r_")
            place_door(d, FLOOR_Z[d["floor"]], None if remise else TAG[d["floor"]],
                       "Remise" if remise else ("Keller" if d["floor"] == "KG" else "Haus"))
        n += 1
    log(f"{n} doors and gates")


def stair_steps(label, folder, rect, top_side, z_top, z_bottom, steps, mat, tag):
    """Solid steps from z_bottom up; top_side = side where the stair starts at z_top (S = low y)."""
    x0, y0, x1, y1 = rect
    rise = (z_top - z_bottom) / (steps + 1)
    length = (y1 - y0) if top_side in ("S", "N") else (x1 - x0)
    g = length / steps
    for i in range(1, steps + 1):
        top = z_top - i * rise
        if top_side == "S":
            r = (x0, y0 + (i - 1) * g, x1, y0 + i * g)
        elif top_side == "N":
            r = (x0, y1 - i * g, x1, y1 - (i - 1) * g)
        elif top_side == "W":
            r = (x0 + (i - 1) * g, y0, x0 + i * g, y1)
        else:
            r = (x1 - i * g, y0, x1 - (i - 1) * g, y1)
        box(f"{label}_{i:02d}", folder, r, z_bottom, top, mat, tag)


def build_stairs():
    stair_steps("Kellertreppe", "Keller", (29.15, 17.4, 30.65, 21.85), "S", 0.0, FLOOR_Z["KG"], 15, "MI_Item_Wood_Oak", TAG["KG"])
    # Outside cellar stair: pit floor, steps, retaining walls with a parapet above the garden.
    box("Aussentreppe_Boden", "Aussen", (44.15, 24.3, 45.75, 30.15), FLOOR_Z["KG"] - 30.0, FLOOR_Z["KG"], "MI_Item_Ground_Concrete")
    stair_steps("Aussentreppe", "Aussen", (44.2, 24.4, 45.6, 28.0), "S", 0.0, FLOOR_Z["KG"], 14, "Paving", None)
    box("Aussentreppe_Mauer_Ost", "Aussen", (45.65, 24.3, 45.85, 30.25), FLOOR_Z["KG"], 95.0, "Plaster")
    box("Aussentreppe_Mauer_Nord", "Aussen", (44.15, 30.05, 45.85, 30.25), FLOOR_Z["KG"], 95.0, "Plaster")
    log("stairs built")


def build_rooms():
    n = 0
    for floor in ("EG", "KG"):
        for r in P.rooms_on(floor):
            x0, y0, x1, y1 = r["rect"]
            vol = spawn(unreal.DCRoomVolume, ue((x0 + x1) / 2, (y0 + y1) / 2, FLOOR_Z[floor] + 120.0))
            name = r["name"].replace(P.SHY, "")
            if r["id"] == "kg_treppe":
                name = "Kellertreppe"
            try_set(vol, "room_name", name)
            try_set(vol, "half_size", unreal.Vector2D((y1 - y0) * 50.0, (x1 - x0) * 50.0))
            try_set(vol, "outdoors", False)
            vol.set_actor_label(f"Raum_{r['id']}")
            remise = r["building"] == "Remise"
            vol.set_folder_path("Villa/Raeume")
            if not remise:
                try_set(vol, "tags", [unreal.Name(TAG[floor])])
            n += 1
    for label, name, rect in (("Raum_Garten", "Garten", V.PLOT), ("Raum_Strasse", "Straße", (-6.0, -9.0, 60.6, 0.0)),
                              ("Raum_Aussentreppe", "Kelleraußentreppe", (44.15, 24.3, 45.75, 30.15))):
        x0, y0, x1, y1 = rect
        z = FLOOR_Z["KG"] + 120.0 if name == "Kelleraußentreppe" else 120.0
        vol = spawn(unreal.DCRoomVolume, ue((x0 + x1) / 2, (y0 + y1) / 2, z))
        try_set(vol, "room_name", name)
        try_set(vol, "half_size", unreal.Vector2D((y1 - y0) * 50.0, (x1 - x0) * 50.0))
        try_set(vol, "outdoors", True)
        vol.set_actor_label(label)
        vol.set_folder_path("Villa/Raeume")
        n += 1
    log(f"{n} rooms")


def build_things():
    for label, item, (x, y), facing, z, floor, locked, contents in THINGS:
        if isinstance(facing, str):
            yaw = PAINTING_FACE[facing[0]] if facing.endswith("@") else FACE[facing]
        else:
            yaw = facing
        base = FLOOR_Z.get(floor, 0.0) if floor else 0.0
        p = spawn(unreal.DCItemPickup, ue(x, y, base + z + 1.0), yaw)
        try_set(p, "item_id", item)
        try_set(p, "locked", locked)
        try_set(p, "open", False)
        specs = []
        for spec_item, inside, spec_locked in contents:
            spec = unreal.DCContentSpec()
            spec.set_editor_property("item_id", spec_item)
            spec.set_editor_property("inside", inside)
            spec.set_editor_property("locked", spec_locked)
            specs.append(spec)
        try_set(p, "contents", specs)
        p.set_actor_label(label)
        p.set_folder_path("Villa/Gegenstaende")
        if floor:
            try_set(p, "tags", [unreal.Name(TAG[floor])])
    for label, floor, rect, z0, z1, mat in DECO_BOXES:
        base = FLOOR_Z.get(floor, 0.0) if floor else 0.0
        box(label, "Moebel", rect, base + z0, base + z1, mat, TAG[floor] if floor else None, collision=z0 < 30)
    log(f"{len(THINGS)} things, {len(DECO_BOXES)} fixed pieces")


# ---------------------------------------------------------------------------------------------------------
# Outside
# ---------------------------------------------------------------------------------------------------------

SURFACE_MAT = {"strasse": "Asphalt", "gehweg": "Paving", "feldweg": "Gravel", "nachbar": "Grass", "kies": "Gravel",
               "pflaster": "Paving", "trittstein": "Paving", "stein": "Stone", "holz": "Deck", "wasser": "PoolTiles",
               "beet": "Soil"}
SURFACE_Z = {"nachbar": 0.3, "strasse": 0.4, "gehweg": 0.6, "feldweg": 0.5, "kies": 0.7, "pflaster": 0.9,
             "trittstein": 1.1, "stein": 1.3, "holz": 1.6, "beet": 1.9, "wasser": 2.2}


def build_ground():
    holes = [HOUSE, OUTSIDE_STAIR_PIT]
    for i, r in enumerate(subtract((-6.0, -9.0, 80.0, 52.0), holes)):
        box(f"Gelaende_{i}", "Aussen", r, -30.0, 0.0, "Grass")
    n = 0
    for s in V.SURFACES:
        kind = s["kind"]
        shape = s["shape"]
        mat = SURFACE_MAT[kind]
        z1 = SURFACE_Z[kind]
        label = f"Flaeche_{kind}_{n}"
        n += 1
        if shape[0] == "rect":
            rect = shape[1]
            if kind == "wasser" and s.get("label") == "Pool":
                build_pool(rect)
                continue
            if kind == "holz" and s.get("label") == "Pavillon":
                box(label, "Aussen", rect, 0.0, 12.0, mat, collision=False)
                for j, (px, py) in enumerate(((rect[0], rect[1]), (rect[2], rect[1]), (rect[0], rect[3]), (rect[2], rect[3]))):
                    px += 0.15 if px == rect[0] else -0.15
                    py += 0.15 if py == rect[1] else -0.15
                    disc(f"Pavillon_Pfosten_{j}", "Aussen", px, py, 0.08, 0.0, 260.0, "MI_Item_Wood_Walnut", collision=True)
                continue
            if kind == "beet" and rect[2] - rect[0] < 1.5 and rect[3] - rect[1] > 3.0 and rect[0] > 15 and rect[0] < 19:
                # raised herb beds
                box(label, "Aussen", rect, 0.0, 40.0, "Soil")
                continue
            box(label, "Aussen", rect, 0.0, z1, mat, collision=False, shadow=False)
        elif shape[0] == "circle":
            cx, cy, r = shape[1]
            if kind == "wasser":
                disc("Brunnen_Becken", "Aussen", cx, cy, r, 0.0, 45.0, "Stone", collision=True)
                disc("Brunnen_Wasser", "Aussen", cx, cy, r - 0.15, 45.0, 47.0, "Water")
            else:
                disc(label, "Aussen", cx, cy, r, 0.0, z1, mat)
        elif shape[0] == "ring":
            cx, cy, ro, _ = shape[1]
            disc(label, "Aussen", cx, cy, ro, 0.0, z1, mat)
        elif shape[0] == "path":
            pts, width = shape[1], shape[2]
            for j, ((ax_, ay_), (bx_, by_)) in enumerate(zip(pts, pts[1:])):
                if kind == "trittstein":
                    length = math.hypot(bx_ - ax_, by_ - ay_)
                    k = max(1, int(length / 0.9))
                    for q in range(k + 1):
                        px = ax_ + (bx_ - ax_) * q / k
                        py = ay_ + (by_ - ay_) * q / k
                        disc(f"{label}_{j}_{q}", "Aussen", px, py, width / 2 * 0.8, 0.0, z1, mat)
                    continue
                hw = width / 2
                rect = (min(ax_, bx_) - hw, min(ay_, by_) - hw, max(ax_, bx_) + hw, max(ay_, by_) + hw)
                if abs(ax_ - bx_) < 1e-6 or abs(ay_ - by_) < 1e-6:
                    box(f"{label}_{j}", "Aussen", rect, 0.0, z1, mat, collision=False, shadow=False)
    log(f"{n} surfaces")


def build_pool(rect):
    x0, y0, x1, y1 = rect
    box("Pool_Rand", "Aussen", (x0 - 0.4, y0 - 0.4, x1 + 0.4, y1 + 0.4), 0.0, 4.0, "Stone", collision=False)
    box("Pool_Becken", "Aussen", rect, 4.0, 5.0, "PoolTiles", collision=False)
    box("Pool_Wasser", "Aussen", (x0 + 0.02, y0 + 0.02, x1 - 0.02, y1 - 0.02), 5.0, 8.0, "Water", collision=False, shadow=False)
    # nobody walks over the water
    box("Pool_Sperre", "Aussen", rect, 0.0, 100.0, "Stone", hidden=True, shadow=False)
    for i, lx in enumerate((30.0, 32.0, 34.0, 36.0)):
        box(f"Liege_{i}", "Aussen", (lx, 41.5, lx + 0.7, 42.9), 0.0, 35.0, "MI_Item_Wood_Oak")


def build_hedges_fence():
    for i, h in enumerate(V.HEDGES):
        box(f"Hecke_{i}", "Aussen", h["rect"], 0.0, h["height"] * 100.0, "Hedge")
    gates = [d for d in V.DOORS if d["floor"] == "Aussen"]
    pts = V.FENCE
    n = 0
    for (ax0, ay0), (ax1, ay1) in zip(pts, pts[1:]):
        horizontal = abs(ay0 - ay1) < 1e-6
        a, b = (min(ax0, ax1), max(ax0, ax1)) if horizontal else (min(ay0, ay1), max(ay0, ay1))
        c = ay0 if horizontal else ax0
        pieces = [(a, b)]
        for g in gates:
            gax, gc, g0, g1, _ = P.door_frame(g)
            if (gax == "h") != horizontal or abs(gc - c) > 1e-6:
                continue
            nxt = []
            for p0, p1 in pieces:
                if g1 <= p0 or g0 >= p1:
                    nxt.append((p0, p1))
                    continue
                if g0 > p0:
                    nxt.append((p0, g0))
                if g1 < p1:
                    nxt.append((g1, p1))
            pieces = nxt
        for p0, p1 in pieces:
            for z0, z1 in ((12.0, 18.0), (100.0, 106.0), (172.0, 180.0)):
                r = (p0, c - 0.02, p1, c + 0.02) if horizontal else (c - 0.02, p0, c + 0.02, p1)
                box(f"Zaun_Riegel_{n}", "Zaun", r, z0, z1, "MI_Item_Iron_Dark", collision=False)
                n += 1
            r = (p0, c - 0.03, p1, c + 0.03) if horizontal else (c - 0.03, p0, c + 0.03, p1)
            box(f"Zaun_Sperre_{n}", "Zaun", r, 0.0, 180.0, "MI_Item_Iron_Dark", hidden=True, shadow=False)
            k = max(1, int((p1 - p0) / 2.5))
            gate_edges = [e for g in gates for e in P.door_frame(g)[2:4]]
            for q in range(k + 1):
                v = p0 + (p1 - p0) * q / k
                if any(abs(v - e) < 0.3 for e in gate_edges) and 0.5 < v < (60.0 if horizontal else 48.0) - 0.5:
                    continue  # the gate's own frame is the post; a post here would narrow the way through
                px, py = (v, c) if horizontal else (c, v)
                box(f"Zaun_Pfosten_{n}_{q}", "Zaun", (px - 0.05, py - 0.05, px + 0.05, py + 0.05), 0.0, 185.0, "MI_Item_Iron_Dark")
            # pickets every 25 cm on the street side only (thin, no collision); behind the hedges posts and rails do
            if not (horizontal and abs(c) < 1e-6):
                continue
            k = max(1, int((p1 - p0) / 0.25))
            for q in range(1, k):
                v = p0 + (p1 - p0) * q / k
                px, py = (v, c) if horizontal else (c, v)
                box(f"Zaun_Stab_{n}_{q}", "Zaun", (px - 0.012, py - 0.012, px + 0.012, py + 0.012), 12.0, 178.0,
                    "MI_Item_Iron_Dark", collision=False, shadow=False)
    log(f"hedges and fence ({n} fence pieces)")


def build_trees():
    for i, (x, y, r, kind) in enumerate(V.TREES):
        trunk_h = 180.0 if kind == "obst" else 320.0
        disc(f"Baum_{i}_Stamm", "Baeume", x, y, 0.16 if kind != "obst" else 0.1, 0.0, trunk_h, "MI_Item_Wood_Walnut", collision=True)
        if kind == "nadel":
            a = mesh_actor(CONE, ue(x, y, 100.0 + r * 125.0), unreal.Vector(r * 2.0, r * 2.0, r * 2.5), "Leaves")
            finish(a, f"Baum_{i}_Krone", "Baeume", collision=False)
        else:
            height = r * 1.5
            a = mesh_actor(SPHERE, ue(x, y, trunk_h + height * 40.0), unreal.Vector(r * 1.9, r * 1.9, height), "Leaves")
            finish(a, f"Baum_{i}_Krone", "Baeume", collision=False)
    log(f"{len(V.TREES)} trees")


def point_light(label, folder, location, lumens, radius, temperature=2700.0, shadows=True, color=None, tag=None):
    light = spawn(unreal.PointLight, location)
    lc = light.get_component_by_class(unreal.PointLightComponent)
    try_set(lc, "intensity_units", unreal.LightUnits.LUMENS)
    try_set(lc, "intensity", lumens)
    if color:
        lc.set_light_color(unreal.LinearColor(color[0], color[1], color[2], 1.0))
    else:
        try_set(lc, "use_temperature", True)
        try_set(lc, "temperature", temperature)
    try_set(lc, "attenuation_radius", radius)
    try_set(lc, "cast_shadows", shadows)
    light.set_actor_label(label)
    light.set_folder_path(f"Villa/{folder}")
    if tag:
        try_set(light, "tags", [unreal.Name(tag)])
    return light


def spot_light(label, folder, location, yaw, pitch, lumens, radius, inner=25.0, outer=55.0, on=True, tags=()):
    light = spawn(unreal.SpotLight, location, yaw, pitch)
    lc = light.get_component_by_class(unreal.SpotLightComponent)
    try_set(lc, "intensity_units", unreal.LightUnits.LUMENS)
    try_set(lc, "intensity", lumens)
    try_set(lc, "use_temperature", True)
    try_set(lc, "temperature", 4200.0)
    try_set(lc, "attenuation_radius", radius)
    try_set(lc, "inner_cone_angle", inner)
    try_set(lc, "outer_cone_angle", outer)
    try_set(lc, "cast_shadows", True)
    if not on:
        try_set(lc, "visible", False)
    light.set_actor_label(label)
    light.set_folder_path(f"Villa/{folder}")
    if tags:
        try_set(light, "tags", [unreal.Name(t) for t in tags])
    return light


def build_lights():
    for i, (x, y, kind, r, note) in enumerate(V.LIGHTS):
        label = f"Aussenlicht_{i}_{kind}"
        if kind == "laterne":
            disc(f"{label}_Mast", "Licht", x, y, 0.06, 0.0, 250.0, "MI_Item_Iron_Dark", collision=True)
            box(f"{label}_Kopf", "Licht", (x - 0.13, y - 0.13, x + 0.13, y + 0.13), 250.0, 285.0, "MI_Item_Bulb", collision=False)
            point_light(label, "Licht", ue(x, y, 265.0), 900.0, r * 140.0)
        elif kind == "poller":
            disc(f"{label}_Poller", "Licht", x, y, 0.08, 0.0, 65.0, "MI_Item_Iron_Dark", collision=True)
            disc(f"{label}_Glas", "Licht", x, y, 0.07, 65.0, 72.0, "MI_Item_Bulb")
            point_light(label, "Licht", ue(x, y, 60.0), 160.0, r * 130.0, shadows=False)
        elif kind == "wand":
            box(f"{label}_Leuchte", "Licht", (x - 0.1, y - 0.1, x + 0.1, y + 0.1), 205.0, 230.0, "MI_Item_Bulb", collision=False)
            point_light(label, "Licht", ue(x, y, 215.0), 420.0, r * 130.0)
        elif kind == "bm":
            box(f"{label}_Strahler", "Licht", (x - 0.12, y - 0.12, x + 0.12, y + 0.12), 255.0, 275.0, "MI_Item_Steel", collision=False)
            aim = BM_AIM.get(note, "S")
            spot_light(label, "Licht", ue(x, y, 260.0), AIM[aim], -55.0, 1600.0, r * 120.0, 30.0, 60.0, on=False,
                       tags=("DCMotionLight",))
        elif kind == "pool":
            point_light(label, "Licht", ue(x, y, 6.0), 260.0, r * 110.0, shadows=False, color=(0.45, 0.8, 1.0))
        elif kind == "boden":
            box(f"{label}_Strahler", "Licht", (x - 0.08, y - 0.08, x + 0.08, y + 0.08), 0.0, 12.0, "MI_Item_Steel", collision=False)
            spot_light(label, "Licht", ue(x, y, 14.0), 0.0, 80.0, 500.0, r * 160.0, 15.0, 40.0)
    for label, floor, (x, y), h, lumens, radius in INDOOR_LIGHTS:
        base = FLOOR_Z[floor] if floor else 0.0
        point_light(label, "Licht", ue(x, y, base + h), lumens, radius, tag=TAG[floor] if floor else None)
    log(f"{len(V.LIGHTS)} outdoor lights, {len(INDOOR_LIGHTS)} indoor lights")


def build_neighbour():
    x0, y0, x1, y1 = V.NEIGHBOUR_HOUSE
    t = 0.3
    for i, r in enumerate(((x0, y0, x1, y0 + t), (x0, y1 - t, x1, y1), (x0, y0, x0 + t, y1), (x1 - t, y0, x1, y1))):
        box(f"Nachbarhaus_Wand_{i}", "Nachbar", r, 0.0, 300.0, "Plaster")
    box("Nachbarhaus_Boden", "Nachbar", (x0, y0, x1, y1), 0.0, 1.0, "MI_Item_Floor_Planks", collision=False)


# ---------------------------------------------------------------------------------------------------------

def main():
    unreal.EditorAssetLibrary.does_asset_exist(SOURCE)
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET) and not unreal.EditorAssetLibrary.delete_asset(TARGET):
        log("could not delete the old map - delete Content/DerClou/Maps/L_Villa01.umap first")
        return
    if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET):
        log("could not copy the source map (delete Content/DerClou/Maps/L_Villa01.umap first)")
        return
    build_materials()
    world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET)
    if not world:
        log("could not load the target map")
        return
    removed = 0
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        if a.get_actor_label().startswith(REMOVE_PREFIXES):
            actor_sub().destroy_actor(a)
            removed += 1
    log(f"removed {removed} actors of the old house")
    by_label = {a.get_actor_label(): a for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)}

    build_ground()
    build_slabs()
    for floor in ("EG", "KG"):
        build_walls(floor)
    build_doors()
    build_stairs()
    build_rooms()
    build_things()
    build_hedges_fence()
    build_trees()
    build_lights()
    build_neighbour()

    for label, (x, y) in CREW.items():
        a = by_label.get(label)
        if a:
            a.set_actor_location(ue(x, y, 93.0), False, False)
            a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0), False)
    n = by_label.get("Nachbar")
    if n:
        n.set_actor_location(ue(NEIGHBOUR[0], NEIGHBOUR[1], 1.0), False, False)
    ps = by_label.get("PlayerStart")
    if ps:
        ps.set_actor_location(ue(16.0, 4.0, 100.0), False, False)
    nav = by_label.get("NavBounds")
    if nav:
        nav.set_actor_location(ue(29.0, 21.5, -25.0), False, False)
        nav.set_actor_scale3d(unreal.Vector(31.0, 31.5, 4.0))
    recast = by_label.get("RecastNavMesh-Default")
    if recast:
        try_set(recast, "force_rebuild_on_load", True)

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET)
    log(f"saved {TARGET}: {ok}")


main()
