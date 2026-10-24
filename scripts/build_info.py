# Automatically embed Git revision information into every PlatformIO build.
# The build number is the Git commit count (not a count of compiler invocations).
Import("env")
import subprocess
from pathlib import Path

root = Path(env.subst("$PROJECT_DIR"))

def git(*args):
    try:
        return subprocess.check_output(["git", *args], cwd=str(root), stderr=subprocess.DEVNULL, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return None

sha = git("rev-parse", "--short=7", "HEAD") or "unknown"
count = git("rev-list", "--count", "HEAD") or "0"
dirty = git("status", "--porcelain", "--untracked-files=no")
if dirty:
    sha += "-dirty"

# Quote as C string literals; keep the current version independent of Git.
env.Append(CPPDEFINES=[
    ("HEATCONTROL_GIT_SHA", '\\"' + sha + '\\"'),
    ("HEATCONTROL_BUILD_NUMBER", '\\"' + count + '\\"'),
])
print("HeatControl firmware: Git %s | Build %s" % (sha, count))
