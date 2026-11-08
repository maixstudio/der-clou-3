"""Builds L_House01: a small house with garden, hallway, office and bedroom (fog test level).

  UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript -script="Scripts/Levels/build_house.py"

Run with the editor closed. Starts from a fresh copy of L_Room01 (sky, moon, post process, crew,
neighbour, nav mesh actors), removes that room and builds the house. Everything this script places
lives in its own Outliner folders and the whole map is rebuilt on every run.

Layout (cm, seen from above, +X = away from the garden):

    X=950  +-----------------+-----------------+
           |      Büro       |  Schlafzimmer   |
    X=350  +--[Bürotür]------+--[Schlafz.tür]--+
           |               Flur                |
    X=0    +------------[Hintertür]------------+
                        Garten (start)
           Y=0             Y=550             Y=1100
"""

import unreal

SOURCE = "/Game/DerClou/Maps/L_Room01"
TARGET = "/Game/DerClou/Maps/L_House01"
CUBE = "/Engine/BasicShapes/Cube.Cube"
MAT = "/Game/DerClou/Items/Materials/MI_Item_"
DECO = "/Game/DerClou/Items/Meshes/SM_Deco_"

WALL_H = 250.0
WALL_T = 20.0
DOOR_W = 100.0
DOOR_H = 210.0

# Walls: (label, start (x, y), end (x, y)) along the centre line; door gaps are separate pieces.
WALLS = [
    ("Wand_Garten_A", (0, -10), (0, 500)),
    ("Wand_Garten_B", (0, 600), (0, 1110)),
    ("Wand_Nord", (950, -10), (950, 1110)),
    ("Wand_West", (-10, 0), (960, 0)),
    ("Wand_Ost", (-10, 1100), (960, 1100)),
    ("Wand_Flur_A", (350, 10), (350, 200)),
    ("Wand_Flur_B", (350, 300), (350, 800)),
    ("Wand_Flur_C", (350, 900), (350, 1090)),
    ("Wand_Buero_Schlafzimmer", (360, 550), (940, 550)),
]
# Lintels above the door openings: (label, x, y from, y to).
LINTELS = [("Sturz_Hintertuer", 0, 500, 600), ("Sturz_Buero", 350, 200, 300), ("Sturz_Schlafzimmer", 350, 800, 900)]
# Doors: label, hinge (x, y), name, locked, difficulty, pick seconds, lock text.
DOORS = [
    ("Tuer_Hintertuer", (0, 500), "Hintertür", True, 3, 4.0, "Einfaches Zylinderschloss"),
    ("Tuer_Buero", (350, 200), "Bürotür", True, 5, 6.0, "Sicherheitsschloss"),
    ("Tuer_Schlafzimmer", (350, 800), "Schlafzimmertür", False, 2, 3.0, "Zimmerschloss"),
]
# Rooms: label, name, centre (x, y), half size (x, y), outdoors.
ROOMS = [
    ("Raum_Garten", "Garten", (-300, 550), (290, 640), True),
    ("Raum_Flur", "Flur", (175, 550), (165, 540), False),
    ("Raum_Buero", "Büro", (650, 275), (290, 265), False),
    ("Raum_Schlafzimmer", "Schlafzimmer", (650, 825), (290, 265), False),
]
# Things: label, item id, (x, y), yaw, locked, open, contents [(item id, inside path, locked)].
THINGS = [
    # Garden: what the crew brought along.
    ("Item_Rucksack", "backpack", (-160, 300), 0, False, False, []),
    ("Item_Werkzeugweste", "tool_vest", (-160, 200), 0, False, False, []),
    ("Item_Brecheisen", "crowbar", (-220, 390), 30, False, False, []),
    # Hallway.
    ("Item_Kommode", "dresser", (35, 250), 90, False, False, [("cash", "", False), ("flashlight", "", False)]),
    ("Item_Gemaelde", "painting", (335, 1000), 0, False, False, []),
    # Office.
    ("Item_Schreibtisch", "desk", (700, 45), 0, False, False,
     [("desk_drawer", "", True), ("laptop", "desk_drawer", False), ("files", "desk_drawer", False)]),
    ("Item_Buerostuhl", "chair", (700, 125), 180, False, False, []),
    ("Item_Wandtresor", "safe", (880, 40), 0, True, False, [("watch", "", False), ("necklace", "", False), ("cash", "", False)]),
    # Bedroom.
    ("Item_Bett", "bed", (800, 900), 0, False, False, [("cash", "", False)]),
    ("Item_Nachttisch", "nightstand", (850, 775), 0, False, False,
     [("jewelry_box", "", True), ("ring", "jewelry_box", False), ("brooch", "jewelry_box", False)]),
    ("Item_Kleiderschrank", "wardrobe", (560, 1055), 180, False, False,
     [("coat", "", False), ("wallet", "coat", False), ("cash", "coat/wallet", False)]),
]
# Lamps: label, kind, deco mesh, mesh location, light location, light rotation (pitch, yaw, roll), lumens, radius.
LAMPS = [
    ("Schreibtischlampe", "spot", "desk_lamp", (650, 25, 75.0), (650, 27, 103.0), (-78.0, 90.0, 0.0), 650.0, 520.0),
    ("Stehlampe_Schlafzimmer", "point", "floor_lamp", (915, 1070, 0.5), (915, 1070, 150.0), None, 480.0, 600.0),
    ("Flurlampe", "point", None, None, (175, 550, 235.0), None, 300.0, 520.0),
]
CREW = {"Crew_Vic": (-300, 480), "Crew_Rosa": (-300, 620)}
NEIGHBOUR = (650, -250)
REMOVE_PREFIXES = ("Wall_", "Floor_Room", "Door_", "Item_", "Deko_", "Licht_Schreibtischlampe", "Licht_Stehlampe")


def log(msg):
    unreal.log_warning(f"[house] {msg}")


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as err:
        log(f"  {type(obj).__name__}.{prop}: {err}")
        return False


def actor_sub():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def spawn(cls, location, yaw=0.0, pitch=0.0, roll=0.0):
    return actor_sub().spawn_actor_from_class(cls, unreal.Vector(*location), unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))


def cube(label, folder, center, size, material=None):
    """A box from the engine cube (100 cm): centre and full size in cm."""
    a = spawn(unreal.StaticMeshActor, center)
    smc = a.static_mesh_component
    smc.set_static_mesh(unreal.load_asset(CUBE))
    a.set_actor_scale3d(unreal.Vector(size[0] / 100.0, size[1] / 100.0, size[2] / 100.0))
    if material:
        mi = unreal.load_asset(MAT + material)
        if mi:
            smc.set_material(0, mi)
    a.set_actor_label(label)
    a.set_folder_path(folder)
    return a


def wall(label, start, end):
    (x0, y0), (x1, y1) = start, end
    if abs(x1 - x0) > abs(y1 - y0):
        size = (abs(x1 - x0), WALL_T, WALL_H)
    else:
        size = (WALL_T, abs(y1 - y0), WALL_H)
    cube(label, "Haus", ((x0 + x1) / 2.0, (y0 + y1) / 2.0, WALL_H / 2.0), size, "Wallpaper")


def main():
    if unreal.EditorAssetLibrary.does_asset_exist(TARGET):
        unreal.EditorAssetLibrary.delete_asset(TARGET)
    if not unreal.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET):
        log("could not copy the source map")
        return
    world = unreal.EditorLoadingAndSavingUtils.load_map(TARGET)
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)

    # Away with the old room (walls, door, furniture, its lamps); sky, moon, post process stay.
    removed = 0
    for a in actors:
        if a.get_actor_label().startswith(REMOVE_PREFIXES):
            actor_sub().destroy_actor(a)
            removed += 1
    log(f"removed {removed} actors of the old room")
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    by_label = {a.get_actor_label(): a for a in actors}

    # Floor and walls.
    cube("Boden_Haus", "Haus", (475, 550, 0), (950, 1100, 1), "Floor_Planks")
    for label, start, end in WALLS:
        wall(label, start, end)
    for label, x, y0, y1 in LINTELS:
        h = WALL_H - DOOR_H
        cube(label, "Haus", (x, (y0 + y1) / 2.0, DOOR_H + h / 2.0), (WALL_T, y1 - y0, h), "Wallpaper")
    log("walls built")

    # Doors.
    for label, hinge, name, locked, difficulty, seconds, lock_text in DOORS:
        d = spawn(unreal.DCDoor, (hinge[0], hinge[1], 0.0))
        try_set(d, "display_name", name)
        try_set(d, "locked", locked)
        try_set(d, "open", False)
        try_set(d, "lock_difficulty", difficulty)
        try_set(d, "pick_lock_seconds", seconds)
        try_set(d, "lock_description", lock_text)
        d.set_actor_label(label)
        d.set_folder_path("Tueren")
    log("doors placed")

    # Rooms (names on the floor plan).
    for label, name, centre, half, outdoors in ROOMS:
        r = spawn(unreal.DCRoomVolume, (centre[0], centre[1], 120.0))
        try_set(r, "room_name", name)
        try_set(r, "half_size", unreal.Vector2D(half[0], half[1]))
        try_set(r, "outdoors", outdoors)
        r.set_actor_label(label)
        r.set_folder_path("Raeume")
    log("rooms placed")

    # Furniture and loot.
    for label, item, (x, y), yaw, locked, is_open, contents in THINGS:
        p = spawn(unreal.DCItemPickup, (x, y, 1.0), yaw)
        try_set(p, "item_id", item)
        try_set(p, "locked", locked)
        try_set(p, "open", is_open)
        specs = []
        for spec_item, inside, spec_locked in contents:
            spec = unreal.DCContentSpec()
            spec.set_editor_property("item_id", spec_item)
            spec.set_editor_property("inside", inside)
            spec.set_editor_property("locked", spec_locked)
            specs.append(spec)
        try_set(p, "contents", specs)
        p.set_actor_label(label)
        p.set_folder_path("Garten" if x < 0 else "Gegenstaende")
    log("things placed")

    # Lamps.
    for label, kind, mesh_name, mesh_loc, light_loc, light_rot, lumens, radius in LAMPS:
        if mesh_name:
            mesh = unreal.load_asset(DECO + mesh_name)
            if mesh:
                m = spawn(unreal.StaticMeshActor, mesh_loc)
                smc = m.static_mesh_component
                smc.set_static_mesh(mesh)
                smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                try_set(smc, "can_ever_affect_navigation", False)
                m.set_actor_label(f"Deko_{label}")
                m.set_folder_path("Deko")
        rot = light_rot or (0.0, 0.0, 0.0)
        light = spawn(unreal.SpotLight if kind == "spot" else unreal.PointLight, light_loc, rot[1], rot[0], rot[2])
        lc = light.get_component_by_class(unreal.SpotLightComponent if kind == "spot" else unreal.PointLightComponent)
        try_set(lc, "intensity_units", unreal.LightUnits.LUMENS)
        try_set(lc, "intensity", lumens)
        try_set(lc, "use_temperature", True)
        try_set(lc, "temperature", 2700.0)
        try_set(lc, "attenuation_radius", radius)
        try_set(lc, "cast_shadows", True)
        if kind == "spot":
            try_set(lc, "inner_cone_angle", 28.0)
            try_set(lc, "outer_cone_angle", 62.0)
        light.set_actor_label(f"Licht_{label}")
        light.set_folder_path("Licht")
    log("lamps placed")

    # Crew in the garden, neighbour behind the office wall, camera and nav mesh over everything.
    for label, (x, y) in CREW.items():
        a = by_label.get(label)
        if a:
            a.set_actor_location(unreal.Vector(x, y, a.get_actor_location().z), False, False)
    n = by_label.get("Nachbar")
    if n:
        n.set_actor_location(unreal.Vector(NEIGHBOUR[0], NEIGHBOUR[1], n.get_actor_location().z), False, False)
    ps = by_label.get("PlayerStart")
    if ps:
        ps.set_actor_location(unreal.Vector(300, 550, 100), False, False)
    nav = by_label.get("NavBounds")
    if nav:
        nav.set_actor_location(unreal.Vector(200, 550, 100), False, False)
        nav.set_actor_scale3d(unreal.Vector(16.0, 14.0, 2.0))
    recast = by_label.get("RecastNavMesh-Default")
    if recast:
        try_set(recast, "force_rebuild_on_load", True)
    pp = by_label.get("Licht_Nachbearbeitung")
    if pp:
        pp.set_actor_location(unreal.Vector(200, 550, 150), False, False)

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, TARGET)
    log(f"saved {TARGET}: {ok}")


main()
