# For IDE linter compatibility
Import = globals().get("Import")
env = globals().get("env")

Import("env")

# Optional: Add git commit hash as build flag
import subprocess
try:
    commit = subprocess.check_output(
        ["git", "rev-parse", "--short", "HEAD"],
        cwd=env.subst("$PROJECT_DIR"),
        stderr=subprocess.DEVNULL
    ).decode().strip()
    env.Append(CPPDEFINES=[("GIT_COMMIT", f'\\"{commit}\\"')])
except Exception:
    env.Append(CPPDEFINES=[("GIT_COMMIT", '"unknown"')])
