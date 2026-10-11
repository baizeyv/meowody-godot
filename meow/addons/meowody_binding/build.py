"""Run plain scons in the framework directory, keeping output on disk."""
import os
from pathlib import Path
import subprocess
import sys

root, log_path, output_dir = map(Path, sys.argv[1:])
environment = os.environ.copy()
environment.update(MEOWODY_BINDING_BUILD="1", PYTHONIOENCODING="utf-8", VSLANG="1033")
environment["MEOWODY_BINDING_OUTPUT"] = str(output_dir)
with log_path.open("w", encoding="utf-8") as log:
    try:
        code = subprocess.run(
            ["scons"], cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT,
        ).returncode
    except OSError as error:
        log.write(f"Cannot start scons: {error}\n")
        code = 1
sys.exit(code)
