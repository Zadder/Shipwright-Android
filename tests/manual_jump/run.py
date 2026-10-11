"""Compile focused tests of the production jump policy and Player integration.

Usage: python tests/manual_jump/run.py -- c++
Or: python tests/manual_jump/run.py -- path/to/zig cc -x c++ -nostdlib++

The fixture stubs engine services (animation, audio, collision queries and hook
registration), not the functions under test. No ROM or graphics device needed.
This does not replace a full Shipwright build or in-game collision/controller QA.
"""
from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def function(source, name):
    match = re.search(r"^\w[\w *]*\b" + name + r"\([^;]*?\) \{", source, re.M)
    if not match:
        raise AssertionError(f"Function missing: {name}")
    start = source.index("{", match.start())
    depth = 1
    end = start + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    # The C player source uses `this`, a reserved C++ keyword.
    return re.sub(r"\bthis\b", "self", source[match.start():end])


def main():
    player = (ROOT / "soh/src/overlays/actors/ovl_player_actor/z_player.c").read_text(encoding="utf-8")
    module = (ROOT / "soh/soh/Enhancements/ManualJump/ManualJump.cpp").read_text(encoding="utf-8")
    module = re.sub(r"^#include .*\n", "", module, flags=re.M)
    constants = "\n".join(
        line for line in (ROOT / "soh/include/z64player.h").read_text(encoding="utf-8").splitlines()
        if line.startswith("#define PLAYER_STATE")
    )
    constants += "\n" + next(
        line for line in (ROOT / "soh/soh/OTRGlobals.h").read_text(encoding="utf-8").splitlines()
        if line.startswith("#define BTN_CUSTOM_JUMP ")
    )
    constants += "\n" + "\n".join(
        line for line in (ROOT / "soh/include/z64actor.h").read_text(encoding="utf-8").splitlines()
        if line.startswith("#define BGCHECKFLAG_")
    )
    bgcheck = (ROOT / "soh/include/z64bgcheck.h").read_text(encoding="utf-8")
    constants += "\n" + re.search(r"typedef enum FloorProperty \{.*?\} FloorProperty;", bgcheck, re.S).group()
    functions = "\n".join(function(player, name) for name in (
        "func_80838940", "func_8083A4A8", "Player_ActionHandler_12", "Player_TryActionHandlerList", "func_8083AA10"
    ))
    pad = (ROOT / "soh/src/code/padmgr.c").read_text(encoding="utf-8")
    edges = "\n".join(line.strip() for line in pad.splitlines()
                      if "input->press.button |=" in line or "input->rel.button |=" in line)
    fixture = (Path(__file__).parent / "harness.cpp.in").read_text(encoding="utf-8")
    fixture = fixture.replace("@CONSTANTS@", constants).replace("@MODULE@", module)
    fixture = fixture.replace("@FUNCTIONS@", functions).replace("@EDGES@", edges)
    out = ROOT / "build/manual-jump-tests"
    out.mkdir(parents=True, exist_ok=True)
    source = out / "manual_jump_test.cpp"
    source.write_text(fixture, encoding="utf-8")
    executable = out / ("manual_jump_test.exe" if sys.platform == "win32" else "manual_jump_test")
    compiler = sys.argv[1:]
    if compiler[:1] == ["--"]:
        compiler = compiler[1:]
    subprocess.run((compiler or ["c++"]) + ["-std=c++20", str(source), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    main()
