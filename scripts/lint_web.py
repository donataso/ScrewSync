#!/usr/bin/env python
"""Lint web/client and web/firmware-client/src with ESLint in a Node Docker container.

Same image/cache setup as scripts/test_web.py. `--fix` applies the automatic fixes.

    python3 scripts/lint_web.py [--fix]
"""
import os
import shutil
import subprocess
import sys

IMAGE = "node:22-alpine"

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_WEB = os.path.join(_ROOT, "web")
_FIRMWARE_CLIENT = os.path.join(_WEB, "firmware-client")
_NODE_MODULES_DIR = os.path.join(_ROOT, ".cache", "node_modules")
_NPM_CACHE_DIR = os.path.join(_ROOT, ".cache", "npmcache")

_NPM_INSTALL = "npm install --no-fund --no-audit --prefer-offline"
# Run from /work with an explicit config: ESLint ignores files outside the
# config's base path, and `-c` makes that the working directory.
_ESLINT = "cd /work && firmware-client/node_modules/.bin/eslint -c firmware-client/eslint.config.mjs --max-warnings 0 client firmware-client/src"


def lint_web(fix):
    exe = shutil.which("docker")
    if not exe:
        sys.stderr.write("lint_web: docker not found on PATH\n")
        return False
    os.makedirs(_NODE_MODULES_DIR, exist_ok=True)
    os.makedirs(_NPM_CACHE_DIR, exist_ok=True)
    cmd = [
        exe, "run", "--rm",
        "--user", "%d:%d" % (os.getuid(), os.getgid()),
        "-e", "HOME=/tmp",
        "-e", "npm_config_cache=/work/.npmcache",
        "-v", "%s:/work/firmware-client" % _FIRMWARE_CLIENT,
        "-v", "%s:/work/client" % os.path.join(_WEB, "client"),
        "-v", "%s:/work/firmware-client/node_modules" % _NODE_MODULES_DIR,
        "-v", "%s:/work/client/node_modules" % _NODE_MODULES_DIR,
        "-v", "%s:/work/.npmcache" % _NPM_CACHE_DIR,
        "-w", "/work/firmware-client", IMAGE, "sh", "-c", _NPM_INSTALL + " && " + _ESLINT + (" --fix" if fix else ""),
    ]
    print("lint_web: " + " ".join(cmd))
    try:
        rc = subprocess.call(cmd)
    except OSError as e:
        sys.stderr.write("lint_web: could not start docker: %s\n" % e)
        return False
    if rc != 0:
        sys.stderr.write("lint_web: failed (exit %d)\n" % rc)
        return False
    print("lint_web: passed")
    return True


if __name__ == "__main__":
    sys.exit(0 if lint_web("--fix" in sys.argv[1:]) else 1)
