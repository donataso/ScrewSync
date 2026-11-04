#!/usr/bin/env python
"""Build web/firmware-client (Vite + React) in a Node Docker container.

No Node on host: `npm install` + `npm run build` in node:22-alpine. Mounts
firmware-client/ and client/ under a clean /work, not the whole web/ tree --
that's a pnpm workspace whose own postcss.config.mjs would otherwise get
picked up by Vite's upward PostCSS search and break the build.

    python3 scripts/build_webui.py
"""
import os
import re
import shutil
import subprocess
import sys

IMAGE = "node:22-alpine"

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_WEB = os.path.join(_ROOT, "web")
_FIRMWARE_CLIENT = os.path.join(_WEB, "firmware-client")
_OUT_DIR = os.path.join(_ROOT, ".build", "firmware-client")
_NODE_MODULES_DIR = os.path.join(_ROOT, ".cache", "node_modules")
_NPM_CACHE_DIR = os.path.join(_ROOT, ".cache", "npmcache")
_CONFIG_HPP = os.path.join(_ROOT, "main", "src", "config.hpp")

_NPM_INSTALL = "npm install --no-fund --no-audit --prefer-offline"


def _read_device_name():
    with open(_CONFIG_HPP, encoding="utf-8") as f:
        src = f.read()
    m = re.search(r'#define\s+DEVICE_NAME\s+"([^"]*)"', src)
    if not m:
        sys.stderr.write("build_webui: DEVICE_NAME not found in %s\n" % _CONFIG_HPP)
        sys.exit(1)
    return m.group(1)


def build_ui():
    if not os.path.isdir(_FIRMWARE_CLIENT):
        sys.stderr.write("build_webui: no web/firmware-client directory\n")
        return False
    exe = shutil.which("docker")
    if not exe:
        sys.stderr.write("build_webui: docker not found on PATH -- install Docker Desktop (and start it).\n")
        return False
    os.makedirs(_NODE_MODULES_DIR, exist_ok=True)
    os.makedirs(_NPM_CACHE_DIR, exist_ok=True)
    os.makedirs(_OUT_DIR, exist_ok=True)
    cmd = [
        exe, "run", "--rm",
        "--user", "%d:%d" % (os.getuid(), os.getgid()),
        "-e", "HOME=/tmp",
        "-e", "npm_config_cache=/work/.npmcache",
        "-e", "OUT_DIR=/work/out",
        "-e", "DEVICE_NAME=%s" % _read_device_name(),
        "-v", "%s:/work/firmware-client" % _FIRMWARE_CLIENT,
        "-v", "%s:/work/client" % os.path.join(_WEB, "client"),
        "-v", "%s:/work/firmware-client/node_modules" % _NODE_MODULES_DIR,
        "-v", "%s:/work/.npmcache" % _NPM_CACHE_DIR,
        "-v", "%s:/work/out" % _OUT_DIR,
        "-w", "/work/firmware-client", IMAGE, "sh", "-c", _NPM_INSTALL + " && npm run build",
    ]
    print("build_webui: " + " ".join(cmd))
    try:
        rc = subprocess.call(cmd)
    except OSError as e:
        sys.stderr.write("build_webui: could not start docker: %s\n" % e)
        return False
    if rc != 0:
        sys.stderr.write("build_webui: container build failed (exit %d)\n" % rc)
        return False
    out = os.path.join(_OUT_DIR, "index.html")
    if not os.path.exists(out):
        sys.stderr.write("build_webui: build finished but %s is missing\n" % out)
        return False
    print("build_webui: wrote %s (%d bytes) -- rebuild the firmware to pick it up"
          % (out, os.path.getsize(out)))
    return True


if __name__ == "__main__":
    sys.exit(0 if build_ui() else 1)
