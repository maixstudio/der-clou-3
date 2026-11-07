"""Night look for L_Room01 (run as commandlet after ue_import_items.py, editor closed):

  UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript -script="Art/Items/ue_room_look.py"

Readability first: cool moonlight fills the whole room from above, warm pools come from lamps that
exist in the room (banker's lamp on the desk, two floor lamps). Fixed exposure so the picture does
not pump while the camera moves. Room surfaces get the world-aligned materials, the walls a skirting.
Everything this script adds lives in the Outliner folders "Licht" and "Deko" and is rebuilt on each run.
"""

import unreal

MAP = "/Game/DerClou/Maps/L_Room01"
MAT = "/Game/DerClou/Items/Materials/MI_Item_"
MESH = "/Game/DerClou/Items/Meshes/SM_Deco_"
FOLDERS = ("Licht", "Deko")

# Tuning (all in one place).
MOON = dict(lux=4.2, color=(0.62, 0.72, 1.0), pitch=-68.0, yaw=35.0, source_angle=2.0)
SKY_INTENSITY = 0.5
SKY_COLOR = (0.55, 0.65, 1.0)
EXPOSURE_EV100 = 1.2
LAMP_TEMPERATURE = 2700.0
LAMPS = [
    # label, kind, mesh, mesh location, light location, light rotation, lumens, radius
    ("Schreibtischlampe", "spot", "desk_lamp", (395, 25, 75.0), (395, 27, 103.0), (-78.0, 90.0, 0.0), 650.0, 520.0),
    ("Stehlampe_Sofa", "point", "floor_lamp", (48, 958, 0.5), (48, 958, 150.0), None, 520.0, 600.0),
    ("Stehlampe_Klavierecke", "point", "floor_lamp", (672, 966, 0.5), (672, 966, 150.0), None, 450.0, 600.0),
]
# Skirting: start, yaw, length in metres (mesh runs 1 m along +X, front +Y).
SKIRTING = [((0, 0, 0.5), 0.0, 7.0), ((700, 1000, 0.5), 180.0, 7.0), ((0, 1000, 0.5), -90.0, 10.0),
            ((700, 0, 0.5), 90.0, 4.5), ((700, 550, 0.5), 90.0, 4.5)]
SURFACES = {"Floor_Room": "Floor_Planks", "Floor": "Ground_Concrete", "Wall_Front": "Wallpaper", "Wall_Left": "Wallpaper",
            "Wall_Right": "Wallpaper", "Wall_Back_A": "Wallpaper", "Wall_Back_B": "Wallpaper", "Wall_Back_Lintel": "Wallpaper"}


def log(msg):
    unreal.log_warning(f"[room] {msg}")


def try_set(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as err:  # property names differ between engine versions
        log(f"  {type(obj).__name__}.{prop}: {err}")
        return False


def spawn(cls, location, rotation=(0.0, 0.0, 0.0)):
    loc = unreal.Vector(*location)
    rot = unreal.Rotator(roll=rotation[2], pitch=rotation[0], yaw=rotation[1])
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if sub:
        return sub.spawn_actor_from_class(cls, loc, rot)
    return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)


def destroy(actor):
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if sub:
        sub.destroy_actor(actor)
    else:
        unreal.EditorLevelLibrary.destroy_actor(actor)


def color8(c):
    return unreal.Color(r=int(c[0] * 255), g=int(c[1] * 255), b=int(c[2] * 255), a=255)


def lin(c):
    return unreal.LinearColor(c[0], c[1], c[2], 1.0)


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)

    # Rebuild what earlier runs added.
    for a in actors:
        if str(a.get_folder_path()) in FOLDERS:
            destroy(a)
    actors = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor)
    by_label = {a.get_actor_label(): a for a in actors}

    # Surfaces.
    for label, mat_name in SURFACES.items():
        a = by_label.get(label)
        mi = unreal.load_asset(MAT + mat_name)
        if a and mi:
            a.static_mesh_component.set_material(0, mi)
    log("surfaces set")

    # Daytime template parts off.
    for label in ("SM_SkySphere", "VolumetricCloud"):
        a = by_label.get(label)
        if a:
            a.set_actor_hidden_in_game(True)
            for c in a.get_components_by_class(unreal.SceneComponent):
                c.set_visibility(False, True)

    # Moon.
    sun = by_label.get("DirectionalLight")
    if sun:
        lc = sun.get_component_by_class(unreal.DirectionalLightComponent)
        sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=MOON["pitch"], yaw=MOON["yaw"]), False)
        try_set(lc, "intensity", MOON["lux"])
        try_set(lc, "light_color", color8(MOON["color"]))
        try_set(lc, "use_temperature", False)
        try_set(lc, "atmosphere_sun_light", False)
        try_set(lc, "light_source_angle", MOON["source_angle"])
        try_set(lc, "cast_shadows", True)
        sun.set_actor_label("Mondlicht")
    sky = by_label.get("SkyLight")
    if sky:
        sc = sky.get_component_by_class(unreal.SkyLightComponent)
        cube = unreal.load_asset("/Engine/MapTemplates/Sky/DaylightAmbientCubemap")
        if cube:
            try_set(sc, "source_type", unreal.SkyLightSourceType.SLS_SPECIFIED_CUBEMAP)
            try_set(sc, "cubemap", cube)
        try_set(sc, "real_time_capture", False)
        try_set(sc, "intensity", SKY_INTENSITY)
        try_set(sc, "light_color", color8(SKY_COLOR))
        sc.recapture_sky()
    fog = by_label.get("ExponentialHeightFog")
    if fog:
        fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
        try_set(fc, "fog_density", 0.004)
        try_set(fc, "fog_inscattering_luminance", lin((0.01, 0.015, 0.03)))

    # Lamps: mesh + light.
    for label, kind, mesh_name, mesh_loc, light_loc, light_rot, lumens, radius in LAMPS:
        mesh = unreal.load_asset(MESH + mesh_name)
        if mesh:
            m = spawn(unreal.StaticMeshActor, mesh_loc)
            smc = m.static_mesh_component
            smc.set_static_mesh(mesh)
            smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            try_set(smc, "can_ever_affect_navigation", False)
            m.set_actor_label(f"Deko_{label}")
            m.set_folder_path("Deko")
        cls = unreal.SpotLight if kind == "spot" else unreal.PointLight
        light = spawn(cls, light_loc, light_rot or (0.0, 0.0, 0.0))
        lc = light.get_component_by_class(unreal.SpotLightComponent if kind == "spot" else unreal.PointLightComponent)
        try_set(lc, "intensity_units", unreal.LightUnits.LUMENS)
        try_set(lc, "intensity", lumens)
        try_set(lc, "use_temperature", True)
        try_set(lc, "temperature", LAMP_TEMPERATURE)
        try_set(lc, "attenuation_radius", radius)
        try_set(lc, "source_radius", 4.0 if kind == "spot" else 9.0)
        try_set(lc, "soft_source_radius", 10.0)
        try_set(lc, "cast_shadows", True)
        if kind == "spot":
            try_set(lc, "inner_cone_angle", 28.0)
            try_set(lc, "outer_cone_angle", 62.0)
        light.set_actor_label(f"Licht_{label}")
        light.set_folder_path("Licht")
    log("lamps placed")

    # Skirting boards.
    skirt = unreal.load_asset(MESH + "skirting")
    if skirt:
        for i, (start, yaw, length) in enumerate(SKIRTING):
            m = spawn(unreal.StaticMeshActor, start, (0.0, yaw, 0.0))
            m.set_actor_scale3d(unreal.Vector(length, 1.0, 1.0))
            smc = m.static_mesh_component
            smc.set_static_mesh(skirt)
            smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
            try_set(smc, "can_ever_affect_navigation", False)
            m.set_actor_label(f"Deko_Fussleiste_{i + 1}")
            m.set_folder_path("Deko")

    # Post process: fixed exposure, gentle bloom, vignette, cool shadows / warm highlights.
    pp = spawn(unreal.PostProcessVolume, (350, 500, 150))
    pp.set_actor_label("Licht_Nachbearbeitung")
    pp.set_folder_path("Licht")
    try_set(pp, "unbound", True)
    s = pp.get_editor_property("settings")
    values = [
        ("auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
        ("auto_exposure_apply_physical_camera_exposure", False),
        ("auto_exposure_bias", EXPOSURE_EV100),
        ("bloom_intensity", 0.35),
        ("vignette_intensity", 0.45),
        ("motion_blur_amount", 0.0),
        ("color_saturation", unreal.Vector4(1.0, 1.0, 1.0, 1.05)),
        ("color_gain_shadows", unreal.Vector4(0.94, 0.98, 1.08, 1.0)),
        ("color_gain_highlights", unreal.Vector4(1.06, 1.0, 0.93, 1.0)),
        ("color_contrast", unreal.Vector4(1.0, 1.0, 1.0, 1.05)),
        ("local_exposure_highlight_contrast_scale", 0.75),
        ("local_exposure_shadow_contrast_scale", 0.8),
        ("film_grain_intensity", 0.08),
        ("lumen_final_gather_quality", 1.5),
    ]
    for prop, value in values:
        try_set(s, "override_" + prop, True)
        try_set(s, prop, value)
    pp.set_editor_property("settings", s)
    log("post process set")

    ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
    log(f"saved {ok}")


main()
