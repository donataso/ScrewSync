#!/usr/bin/env python
"""Merge Zephyr/GCC compile_commands.json files into one clang-tidy can parse.

clang rejects GCC-only -f/-m flags, so they are dropped. clang also does not
know the cross g++'s libstdc++ search path, so it is queried and added.

    python3 scripts/gen_tidy_db.py <build_dir>... <out.json>
"""
import glob
import json
import os
import re
import shlex
import subprocess
import sys

_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
_SOURCES = os.path.join(_ROOT, "main") + os.sep


def filtered(args):
    # A module's -DNAME=-1 has no source line, so bugprone-macro-parentheses cannot be suppressed there.
    args = [re.sub(r"^(-D\w+=)(-\d+)$", r"\1(\2)", a) for a in args]
    return [a for a in args if not a.startswith(("-f", "-m"))]


def cxx_isystem_dirs(compiler):
    # GCC's own lib/gcc/<triple>/<ver>/include dirs hold builtin shims clang cannot use.
    out = subprocess.run([compiler, "-E", "-x", "c++", "-v", "/dev/null"], capture_output=True, text=True).stderr
    lines = out.splitlines()
    start = lines.index("#include <...> search starts here:") + 1
    end = lines.index("End of search list.")
    dirs = [line.strip() for line in lines[start:end]]
    return ["-isystem" + d for d in dirs if "/c++/" in d or "/lib/gcc/" not in os.path.normpath(d)]


def main():
    build_dirs = sys.argv[1:-1]
    out_path = sys.argv[-1]
    isystem_cache = {}
    merged = []
    for build_dir in build_dirs:
        for path in glob.glob(build_dir + "/**/compile_commands.json", recursive=True):
            with open(path, encoding="utf-8") as f:
                entries = json.load(f)
            for e in entries:
                if not os.path.realpath(e["file"]).startswith(_SOURCES):
                    continue
                args = shlex.split(e["command"]) if "command" in e else e["arguments"]
                compiler = args[0]
                if compiler not in isystem_cache:
                    isystem_cache[compiler] = cxx_isystem_dirs(compiler)
                merged.append({"directory": e["directory"], "file": e["file"], "arguments": filtered(args) + isystem_cache[compiler]})
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, "w", encoding="utf-8") as f:
        json.dump(merged, f)


if __name__ == "__main__":
    main()
