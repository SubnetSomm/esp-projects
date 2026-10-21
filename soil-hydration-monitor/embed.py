from pathlib import Path
Import("env")
root = Path(env.subst("$PROJECT_DIR"))
html = (root / "web/index.html").read_text()
assert ')SOILPAGE"' not in html
(root / "src/dashboard.h").write_text(
    '#pragma once\n#include <Arduino.h>\nconst char DASHBOARD[] PROGMEM = R"SOILPAGE('
    + html + ')SOILPAGE";\n'
)
