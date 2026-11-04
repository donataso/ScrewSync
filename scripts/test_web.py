#!/usr/bin/env python
"""Type-check and test web/client in a Node Docker container.

No Node on host: `npm install`, then `tsc --noEmit` and `vitest run` in
node:22-alpine, same image/cache setup as scripts/build_webui.py. Also
mounts the repo's tests/ dir (read-only) so web/client/gcode/*.test.ts can
run the shared firmware/web G-code test vectors.

    python3 scripts/test_web.py
"""
import os
import shutil
import subprocess
import sys

IMAGE = "node:22-alpine"

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_WEB = os.path.join(_ROOT, "web")
_FIRMWARE_CLIENT = os.path.join(_WEB, "firmware-client")
_TESTS = os.path.join(_ROOT, "tests")
_NODE_MODULES_DIR = os.path.join(_ROOT, ".cache", "node_modules")
_NPM_CACHE_DIR = os.path.join(_ROOT, ".cache", "npmcache")

_NPM_INSTALL = "npm install --no-fund --no-audit --prefer-offline"
_RUN = _NPM_INSTALL + " && npx tsc --noEmit && npx vitest run"


def test_web():
    if not os.path.isdir(_FIRMWARE_CLIENT):
        sys.stderr.write("test_web: no web/firmware-client directory\n")
        return False
    exe = shutil.which("docker")
    if not exe:
        sys.stderr.write("test_web: docker not found on PATH -- install Docker Desktop (and start it).\n")
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
        "-v", "%s:/work/tests:ro" % _TESTS,
        "-v", "%s:/work/firmware-client/node_modules" % _NODE_MODULES_DIR,
        # tsc resolves node_modules by walking up from each file's own
        # directory, not from the project root -- client/ needs its own
        # view of the same node_modules so imports like "react" resolve.
        "-v", "%s:/work/client/node_modules" % _NODE_MODULES_DIR,
        "-v", "%s:/work/.npmcache" % _NPM_CACHE_DIR,
        "-w", "/work/firmware-client", IMAGE, "sh", "-c", _RUN,
    ]
    print("test_web: " + " ".join(cmd))
    try:
        rc = subprocess.call(cmd)
    except OSError as e:
        sys.stderr.write("test_web: could not start docker: %s\n" % e)
        return False
    if rc != 0:
        sys.stderr.write("test_web: container run failed (exit %d)\n" % rc)
        return False
    print("test_web: passed")
    return True


if __name__ == "__main__":
    sys.exit(0 if test_web() else 1)
