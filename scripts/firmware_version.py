import os
import subprocess

Import("env")


def run_git(args):
    try:
        result = subprocess.check_output(
            ["git", *args],
            cwd=env.subst("$PROJECT_DIR"),
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        return result
    except Exception:
        return ""


fw_version = os.environ.get("FIRMWARE_VERSION", "").strip()
tag = run_git(["describe", "--tags", "--exact-match", "HEAD"])
branch = run_git(["rev-parse", "--abbrev-ref", "HEAD"])
revision = run_git(["rev-parse", "--short", "HEAD"])

if branch == "HEAD":
    branch = ""

if fw_version:
    version = fw_version
elif tag:
    version = tag
elif branch:
    version = branch
else:
    version = f"git-{revision}" if revision else "git-unknown"

env.Append(BUILD_FLAGS=[f"-DFIRMWARE_VERSION='\\\"{version}\\\"'"])
print(f"[firmware_version] FIRMWARE_VERSION={version}")