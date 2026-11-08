# Editor check (no clicking): starts PIE right after the editor loaded its start map (L_Villa01), then checks
# whether points on the plot lie on the nav mesh of the PIE world, and quits. Logs lines starting with "DCNAV".
# Run: UnrealEditor.exe DerClou3.uproject -ExecCmds="py C:/Dev/der-clou-3/Scripts/Tests/pie_nav_probe.py"
import time
import unreal

# Plan metres (x east, y north) -> Unreal cm, as in build_villa.py.
def villa(x, y, z=0.0):
    return unreal.Vector(y * 100.0, x * 100.0, z)

POINTS = {
    "street (crew start)": villa(13.6, -1.1),
    "street west": villa(6.5, -1.6),
    "garden behind gate": villa(32.5, 12.0),
    "garden lawn": villa(20.0, 10.0),
    "living room": villa(40.0, 26.0),
    "hall": villa(29.9, 16.75),
    "cellar corridor": villa(29.9, 23.0, -290.0),
}
CHECK_AT = [2.0, 6.0, 12.0, 20.0, 30.0]

state = {"phase": 0, "pie_start": None, "checks": list(CHECK_AT), "handle": None, "end_at": None}


def log(msg):
    unreal.log_warning("DCNAV " + msg)


def game_world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
    except Exception:
        return None


def probe(world, label):
    ok = 0
    for name, point in POINTS.items():
        result = unreal.NavigationSystemV1.project_point_to_navigation(world, point, None, None, unreal.Vector(80.0, 80.0, 250.0))
        found = result if not isinstance(result, tuple) else (result[1] if result[0] else None)
        hit = found is not None and (not isinstance(found, unreal.Vector) or (found - point).length() < 400.0)
        ok += 1 if hit else 0
        log("%s  %-22s %s" % (label, name, "on nav mesh" if hit else "NO NAV MESH"))
    log("%s  %d/%d points on the nav mesh" % (label, ok, len(POINTS)))


def tick(delta):
    now = time.time()
    if state["phase"] == 0:
        log("requesting PIE right after editor start")
        unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_begin_play()
        state["phase"] = 1
        state["requested"] = now
        return
    if state["phase"] == 1:
        if game_world() is not None:
            state["pie_start"] = now
            state["phase"] = 2
            log("PIE running (%.1f s after request)" % (now - state["requested"]))
        elif now - state["requested"] > 60.0:
            log("PIE did not start")
            state["phase"] = 3
            state["end_at"] = now
        return
    if state["phase"] == 2:
        world = game_world()
        elapsed = now - state["pie_start"]
        if state["checks"] and elapsed >= state["checks"][0]:
            at = state["checks"].pop(0)
            if world is not None:
                probe(world, "t=%4.0fs" % at)
        if not state["checks"]:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).editor_request_end_play()
            state["phase"] = 3
            state["end_at"] = now
        return
    if state["phase"] == 3 and now - state["end_at"] > 3.0:
        log("done, quitting editor")
        unreal.unregister_slate_post_tick_callback(state["handle"])
        unreal.SystemLibrary.quit_editor()
        state["phase"] = 4


state["handle"] = unreal.register_slate_post_tick_callback(tick)
log("probe registered")
