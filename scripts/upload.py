#!/usr/bin/env python3
"""Upload a local directory to the PS5 through PS5 Web File Manager's HTTP API.

Usage: PS5_HOST=192.168.0.134 scripts/upload.py LOCAL_DIR REMOTE_PARENT [--exclude NAME]...

The directory is created as REMOTE_PARENT/<basename of LOCAL_DIR>, overwriting
files that already exist. --exclude skips files and directories with that name.
"""
import json
import os
import sys
import time
import urllib.parse
import urllib.request

HOST = os.environ.get("PS5_HOST")
PORT = os.environ.get("PS5_WFM_PORT", "8888")


def call(path, params=None, data=None, headers=None, form=False):
    url = f"http://{HOST}:{PORT}{path}"
    if form:
        data = urllib.parse.urlencode(params).encode()
        headers = {"Content-Type": "application/x-www-form-urlencoded"}
    elif params:
        url += "?" + urllib.parse.urlencode(params)
    req = urllib.request.Request(url, data=data, headers=headers or {},
                                 method="POST" if data is not None else "GET")
    with urllib.request.urlopen(req, timeout=600) as resp:
        body = resp.read()
    reply = json.loads(body) if body else {"ok": True}
    if not reply.get("ok", True):
        raise RuntimeError(f"{path}: {reply}")
    return reply


def wait_for_task(task_id):
    while True:
        tasks = call("/api/tasks").get("tasks", [])
        task = next((t for t in tasks if t.get("id") == task_id), None)
        if task is None or task.get("state") not in ("running", "pending", "queued"):
            return task
        time.sleep(0.5)


def main():
    args = sys.argv[1:]
    excludes = set()
    while "--exclude" in args:
        i = args.index("--exclude")
        excludes.add(args[i + 1])
        del args[i:i + 2]
    if not HOST or len(args) != 2:
        sys.exit(__doc__)
    local_dir = os.path.abspath(args[0])
    remote_parent = args[1]
    top = os.path.basename(local_dir)

    files = []
    for root, dirs, names in os.walk(local_dir, followlinks=True):
        dirs[:] = sorted(d for d in dirs if d not in excludes)
        for name in sorted(n for n in names if n not in excludes):
            path = os.path.join(root, name)
            rel = os.path.join(top, os.path.relpath(path, local_dir))
            files.append((path, rel, os.path.getsize(path)))
    total = sum(size for _, _, size in files)

    task_id = call("/api/upload/prepare", {
        "path": remote_parent,
        "src": files[0][1],
        "total": total,
        "count": len(files),
        "rels": "\n".join(rel for _, rel, _ in files),
        "sizes": "\n".join(str(size) for _, _, size in files),
        "overwrite": "1",
    }, form=True)["task_id"]

    sent = 0
    try:
        for i, (path, rel, size) in enumerate(files, 1):
            print(f"[{i}/{len(files)}] {rel} ({size:,} bytes)", flush=True)
            with open(path, "rb") as f:
                call("/api/upload-file", data=f.read(), headers={
                    "Content-Type": "application/octet-stream",
                    "X-WFM-Task-ID": str(task_id),
                    "X-WFM-Path": urllib.parse.quote(remote_parent, safe=""),
                    "X-WFM-Rel": urllib.parse.quote(rel, safe=""),
                    "X-WFM-Size": str(size),
                    "X-WFM-Overwrite": "1",
                })
            sent += size
    except BaseException:
        call("/api/cancel", {"id": task_id})
        call("/api/upload/finish", {"task_id": task_id})
        raise

    call("/api/upload/finish", {"task_id": task_id})
    task = wait_for_task(task_id)
    print(f"Uploaded {len(files)} files ({sent:,} bytes) to "
          f"{remote_parent.rstrip('/')}/{top}; task state: "
          f"{task.get('state') if task else 'done'}")


if __name__ == "__main__":
    main()
