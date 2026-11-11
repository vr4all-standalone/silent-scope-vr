"""Stage the local cabinet rifle in the isolated Quest Flycast tree."""
from pathlib import Path
import difflib
import shutil

project = Path(__file__).resolve().parents[1]
engine = Path("D:/VR4ALL-SilentScope-Build/flycast-source")
host = engine / "core/rend/vr/xr_host.cpp"
before = host.read_text(encoding="utf-8")
after = before
edits = [
    ('#include "dreamcast/rifle_renderer.h"',
     '#include "dreamcast/rifle_renderer.h"\n#include "dreamcast/dc_rifle_fit.h"', 1),
    ("false,target.hand});", "false,target.hand,ss::dc_rifle::foregrip});", 1),
    ("glm::vec3(0,.005f,-.94f)", "ss::dc_rifle::muzzle", 2),
    ("std::make_unique<ss::RifleRenderer>()",
     "std::make_unique<ss::RifleRenderer>(ss::dc_rifle::mesh())", 1),
]
for old, new, expected in edits:
    if after.count(old) != expected:
        raise SystemExit(f"Unexpected XR host anchor: {old}")
    after = after.replace(old, new)

files = [project / "native" / name for name in
         ("rifle_renderer.h", "rifle_renderer.cpp", "rifle_support.h")]
files += [project / "native/dreamcast/dc_rifle_fit.h",
          project / "private/rifle-asset-candidate/rifle_cabinet_mesh.h"]
for source in files:
    shutil.copyfile(source, engine / "core/dreamcast" / source.name)
host.write_text(after, encoding="utf-8", newline="\n")
patch = difflib.unified_diff(before.splitlines(keepends=True), after.splitlines(keepends=True),
                            "a/core/rend/vr/xr_host.cpp", "b/core/rend/vr/xr_host.cpp")
(project / "patches/dreamcast/0031-cabinet-rifle-fit.patch").write_text("".join(patch), newline="\n")
