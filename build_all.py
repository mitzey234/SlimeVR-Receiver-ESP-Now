#!/usr/bin/env python3
"""
Build all PlatformIO environments defined in platformio.ini and package
firmware.bin, bootloader.bin, and partitions.bin into per-board zip archives
saved to the 'out' directory.
"""

import configparser
import os
import re
import shutil
import subprocess
import sys
import zipfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).parent.resolve()
INI_FILE = SCRIPT_DIR / "platformio.ini"
BUILD_DIR = SCRIPT_DIR / ".pio" / "build"
OUT_DIR = SCRIPT_DIR / "out"

ARTIFACTS = ["firmware.bin", "bootloader.bin", "partitions.bin"]

# Locate the PlatformIO CLI executable.
# Prefer the penv installation used by VS Code / PlatformIO IDE extension.
_PIO_CANDIDATES = [
    Path.home() / ".platformio" / "penv" / "Scripts" / "platformio.exe",  # Windows
    Path.home() / ".platformio" / "penv" / "bin" / "platformio",           # Linux/macOS
]

def _find_pio() -> str:
    for candidate in _PIO_CANDIDATES:
        if candidate.exists():
            return str(candidate)
    # Fall back to whatever is on PATH
    found = shutil.which("pio") or shutil.which("platformio")
    if found:
        return found
    raise FileNotFoundError(
        "Could not find the PlatformIO executable. "
        "Make sure PlatformIO is installed or add it to your PATH."
    )

PIO_EXE = _find_pio()


def parse_environments(ini_path: Path) -> list[str]:
    """Return all [env:xxx] environment names from platformio.ini."""
    # configparser doesn't handle ';' inline comments or duplicate keys well
    # for platformio.ini, so we pre-process to strip inline comments first.
    content = ini_path.read_text(encoding="utf-8")
    # Remove inline ; comments (not at start of line, which are full-line comments)
    content = re.sub(r"(?<!^)\s*;.*$", "", content, flags=re.MULTILINE)

    parser = configparser.ConfigParser()
    parser.read_string(content)

    envs = []
    for section in parser.sections():
        if section.startswith("env:"):
            envs.append(section[4:])  # strip 'env:' prefix
    return envs


def prepare_out_dir(out_dir: Path) -> None:
    """Create the out directory if needed, or empty it if it already exists."""
    if out_dir.exists():
        print(f"[*] Emptying existing output directory: {out_dir}")
        shutil.rmtree(out_dir)
    out_dir.mkdir(parents=True)
    print(f"[*] Created output directory: {out_dir}")


def build_env(env_name: str) -> bool:
    """Run 'pio run -e <env_name>'. Returns True on success."""
    print(f"\n{'='*60}")
    print(f"[*] Building environment: {env_name}")
    print(f"{'='*60}")
    result = subprocess.run(
        [PIO_EXE, "run", "-e", env_name],
        cwd=SCRIPT_DIR,
    )
    if result.returncode != 0:
        print(f"[!] Build FAILED for environment '{env_name}' (exit code {result.returncode})")
        return False
    return True


def package_artifacts(env_name: str, out_dir: Path) -> bool:
    """
    Zip firmware.bin, bootloader.bin, and partitions.bin from the build
    directory for the given environment into out/<env_name>.zip.
    Returns True if at least firmware.bin was found and zipped.
    """
    env_build_dir = BUILD_DIR / env_name
    zip_path = out_dir / f"{env_name}.zip"

    found = []
    missing = []
    for artifact in ARTIFACTS:
        artifact_path = env_build_dir / artifact
        if artifact_path.exists():
            found.append(artifact_path)
        else:
            missing.append(artifact)

    if not found:
        print(f"[!] No artifacts found for '{env_name}' in {env_build_dir}")
        return False

    if missing:
        print(f"[!] Warning: missing artifacts for '{env_name}': {', '.join(missing)}")

    with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as zf:
        for artifact_path in found:
            zf.write(artifact_path, artifact_path.name)
            print(f"    + {artifact_path.name}")

    print(f"[+] Packaged -> {zip_path.relative_to(SCRIPT_DIR)}")
    return True


def main() -> int:
    print(f"[*] Reading environments from: {INI_FILE.relative_to(SCRIPT_DIR)}")
    envs = parse_environments(INI_FILE)

    if not envs:
        print("[!] No environments found in platformio.ini")
        return 1

    print(f"[*] Found environments: {', '.join(envs)}")

    prepare_out_dir(OUT_DIR)

    results: dict[str, str] = {}

    for env in envs:
        build_ok = build_env(env)
        if build_ok:
            pkg_ok = package_artifacts(env, OUT_DIR)
            results[env] = "OK" if pkg_ok else "BUILD OK / PACKAGE FAILED"
        else:
            results[env] = "BUILD FAILED"

    print(f"\n{'='*60}")
    print("[*] Build summary:")
    for env, status in results.items():
        print(f"    {env:<30} {status}")
    print(f"{'='*60}")

    failed = [e for e, s in results.items() if s != "OK"]
    if failed:
        print(f"\n[!] {len(failed)} environment(s) had errors: {', '.join(failed)}")
        return 1

    print(f"\n[+] All builds succeeded. Archives saved to: {OUT_DIR.relative_to(SCRIPT_DIR)}/")
    return 0


if __name__ == "__main__":
    sys.exit(main())
