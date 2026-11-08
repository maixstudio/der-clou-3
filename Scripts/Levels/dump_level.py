"""Writes every actor of a level (label, class, folder, transform) to Saved/level_dump.txt.

  UnrealEditor-Cmd.exe DerClou3.uproject -run=pythonscript -script="Scripts/Levels/dump_level.py"

Set MAP below (default L_Room01). Handy before writing a level script.
"""

import os

import unreal

MAP = os.environ.get("DC_DUMP_MAP", "/Game/DerClou/Maps/L_Room01")


def main():
    world = unreal.EditorLoadingAndSavingUtils.load_map(MAP)
    lines = []
    for a in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        loc = a.get_actor_location()
        rot = a.get_actor_rotation()
        scale = a.get_actor_scale3d()
        extra = ""
        smc = a.get_component_by_class(unreal.StaticMeshComponent)
        if smc and smc.static_mesh:
            extra = f" mesh={smc.static_mesh.get_name()}"
        lines.append(f"{a.get_actor_label():32s} {a.get_class().get_name():28s} folder={str(a.get_folder_path()):12s} "
                     f"loc=({loc.x:.0f},{loc.y:.0f},{loc.z:.0f}) yaw={rot.yaw:.0f} scale=({scale.x:.2f},{scale.y:.2f},{scale.z:.2f}){extra}")
    path = os.path.join(unreal.Paths.project_saved_dir(), "level_dump.txt")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(sorted(lines)))
    unreal.log_warning(f"[dump] {len(lines)} actors -> {path}")


main()
