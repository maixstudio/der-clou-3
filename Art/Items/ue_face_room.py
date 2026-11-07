"""One-off level fix: furniture models have a front (+Y); turn the pieces at the walls to face the room."""
import unreal

YAW = {"Item_Sofa": 180.0, "Item_Klavier": 180.0, "Item_Kleiderschrank": -90.0, "Item_Sessel": -140.0}
world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/DerClou/Maps/L_Room01")
for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
    yaw = YAW.get(a.get_actor_label())
    if yaw is not None:
        r = a.get_actor_rotation()
        a.set_actor_rotation(unreal.Rotator(r.roll, r.pitch, yaw), False)
        unreal.log_warning(f"[face] {a.get_actor_label()} yaw {yaw}")
ok = unreal.EditorLoadingAndSavingUtils.save_map(world, "/Game/DerClou/Maps/L_Room01")
unreal.log_warning(f"[face] saved {ok}")
