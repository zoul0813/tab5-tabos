"""Exercise production architecture checks against isolated negative fixtures."""
import pathlib
import subprocess
import sys
import tempfile

cmake, tests = sys.argv[1:]
tests = pathlib.Path(tests)


def check(script, path, contents, diagnostic, application=False):
    with tempfile.TemporaryDirectory(prefix="tabos-boundary-") as temporary:
        root = pathlib.Path(temporary)
        source = root / path
        source.parent.mkdir(parents=True)
        source.write_text(contents)
        if application:
            (root / "apps" / path.split("/")[1] / "Makefile").touch()
        result = subprocess.run(
            [cmake, f"-DTABOS_SOURCE_DIR={root}", "-P", str(tests / script)],
            capture_output=True, text=True, check=False,
        )
        if result.returncode == 0 or diagnostic not in result.stderr or str(source) not in result.stderr:
            raise AssertionError(f"{script} missed {path}: {result.stdout}{result.stderr}")
        source.write_text("int valid_symbol(void);\n")
        subprocess.run(
            [cmake, f"-DTABOS_SOURCE_DIR={root}", "-P", str(tests / script)], check=True,
        )


for directory in ("console", "process", "time", "pointer", "camera", "sdk/lib"):
    for header in ("SDL3/SDL.h", "freertos/task.h", "esp_system.h"):
        check("check_boundaries.cmake", f"{directory}/probe.c", f"#include <{header}>\n",
              "Platform header leaked")

for application in ("netutils", "snake", "clock", "graphics_demo", "graphics_benchmark", "starfall", "doom", "kilo", "future_app"):
    check("check_application_api_boundary.cmake", f"apps/{application}/src/probe.c",
          "tabos_elf_api_t* transport;\n", "Application references private ELF ABI", True)

for directory in ("audio", "net", "pointer", "camera", "time"):
    check("check_naming.cmake", f"{directory}/probe.c", "void tab_legacy(void);\n",
          "Legacy internal tab_ symbol")
    check("check_naming.cmake", f"{directory}/include/tabos/internal/probe.h",
          "void tabos_internal_function(void);\n", "Internal header declares public-prefixed function")
