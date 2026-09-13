#!/usr/bin/env python3
"""Build cll_update.json from GitHub latest releases (and optional local files).

Output is consumed by the launcher from:
  https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_update.json

Archives are hashed as the parent; every unpacked file is listed as a child.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
import urllib.error
import urllib.request
import zipfile
from pathlib import Path

UA = "CLL-update-gen/1.1"
HOSTED = "https://mestretm.github.io/CLL-Cod-Lan-Launcher/cll_update.json"

# id, repo, asset name on the latest release, kind
CATALOG = [
    {
        "id": "launcher",
        "repo": "MestreTM/CLL-Cod-Lan-Launcher",
        "asset": "CodLanLauncher.exe",
        "kind": "file",
    },
    {
        "id": "pu",
        "repo": "MestreTM/CLL-Cod-Lan-Launcher",
        "asset": "pu.dat",
        "kind": "archive",
    },
    {
        "id": "t7-cll",
        "repo": "MestreTM/t7-cll",
        "asset": "t7_cll.zip",
        "kind": "archive",
    },
    {
        "id": "s1-cll",
        "repo": "MestreTM/s1-cll",
        "asset": "s1.exe",
        "kind": "file",
    },
]


def latest_url(repo: str, asset: str) -> str:
    return f"https://github.com/{repo}/releases/latest/download/{asset}"


def tag_url(repo: str, tag: str, asset: str) -> str:
    return f"https://github.com/{repo}/releases/download/{tag}/{asset}"


def api_json(url: str):
    req = urllib.request.Request(
        url,
        headers={"User-Agent": UA, "Accept": "application/vnd.github+json"},
    )
    with urllib.request.urlopen(req, timeout=30) as r:
        return json.loads(r.read().decode("utf-8"))


def api_releases(repo: str) -> list[dict]:
    rows = api_json(f"https://api.github.com/repos/{repo}/releases?per_page=100")
    return rows if isinstance(rows, list) else []


def release_asset(rel: dict, asset: str) -> dict | None:
    name = asset.lower()
    for a in rel.get("assets") or []:
        if str(a.get("name", "")).lower() == name:
            return a
    return None


def resolve_release(repo: str, asset: str) -> tuple[str, str, bool]:
    """Newest release that actually contains the asset.

    Returns (tag, download_url, is_latest_release).
    """
    releases = api_releases(repo)
    if not releases:
        return "", latest_url(repo, asset), True
    latest_tag = str(releases[0].get("tag_name") or "")
    for rel in releases:
        if rel.get("draft") or rel.get("prerelease"):
            continue
        found = release_asset(rel, asset)
        if not found:
            continue
        tag = str(rel.get("tag_name") or "")
        url = found.get("browser_download_url") or tag_url(repo, tag, asset)
        return tag, url, tag == latest_tag
    # last resort: include pre-releases
    for rel in releases:
        found = release_asset(rel, asset)
        if not found:
            continue
        tag = str(rel.get("tag_name") or "")
        url = found.get("browser_download_url") or tag_url(repo, tag, asset)
        return tag, url, tag == latest_tag
    return latest_tag, latest_url(repo, asset), True


def md5_file(path: Path) -> str:
    h = hashlib.md5()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def download(url: str, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, timeout=180) as r, dest.open("wb") as out:
        shutil.copyfileobj(r, out)


def download_with_fallback(urls: list[str], dest: Path) -> str:
    last_err = None
    seen = set()
    for url in urls:
        if not url or url in seen:
            continue
        seen.add(url)
        print(f"download {url}")
        try:
            download(url, dest)
            return url
        except urllib.error.HTTPError as exc:
            last_err = exc
            print(f"  {exc} — trying older tag")
    if last_err:
        raise last_err
    raise RuntimeError("no download URL")


def find_7z() -> str | None:
    for name in ("7z", "7z.exe", "7za"):
        p = shutil.which(name)
        if p:
            return p
    return None


def extract_archive(archive: Path, dest: Path) -> bool:
    dest.mkdir(parents=True, exist_ok=True)
    suffix = archive.suffix.lower()
    if suffix == ".zip" or zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as zf:
            zf.extractall(dest)
        return True
    seven = find_7z()
    if seven:
        r = subprocess.run(
            [seven, "x", str(archive), f"-o{dest}", "-y"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        return r.returncode == 0
    return False


def child_entries(root: Path) -> list[dict]:
    out = []
    for p in sorted(root.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(root).as_posix()
        out.append({"path": rel, "md5": md5_file(p), "size": p.stat().st_size})
    return out


def resolve_local(overrides: dict[str, Path], asset: str) -> Path | None:
    if asset in overrides:
        return overrides[asset]
    key = Path(asset).name
    return overrides.get(key)


def build_item(spec: dict, cache: Path, overrides: dict[str, Path]) -> dict:
    repo = spec["repo"]
    asset = spec["asset"]
    tag, asset_url, is_latest = resolve_release(repo, asset)
    # Prefer the floating latest URL when the current release has the file.
    url = latest_url(repo, asset) if is_latest else asset_url

    local = resolve_local(overrides, asset)
    if local and local.is_file():
        src = local
    else:
        src = cache / spec["id"] / asset
        used = download_with_fallback(
            [url, asset_url, tag_url(repo, tag, asset) if tag else ""],
            src,
        )
        if not is_latest:
            url = used

    item = {
        "id": spec["id"],
        "name": asset,
        "repo": repo,
        "version": tag,
        "url": url,
        "md5": md5_file(src),
        "size": src.stat().st_size,
    }

    if spec["kind"] == "archive":
        with tempfile.TemporaryDirectory(prefix="cllupd_") as tmp:
            extracted = extract_archive(src, Path(tmp))
            if extracted:
                children = child_entries(Path(tmp))
                if children:
                    item["files"] = children
            else:
                print(f"warn: could not unpack {asset}", file=sys.stderr)

    return item


def parse_overrides(pairs: list[str]) -> dict[str, Path]:
    out: dict[str, Path] = {}
    for raw in pairs:
        if "=" not in raw:
            p = Path(raw)
            out[p.name] = p
            continue
        name, path = raw.split("=", 1)
        out[name.strip()] = Path(path.strip())
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description="Generate cll_update.json")
    ap.add_argument("-o", "--out", default="cll_update.json")
    ap.add_argument("--cache", default=".cll_update_cache")
    ap.add_argument(
        "--local",
        action="append",
        default=[],
        help="Use a local file instead of downloading. name=path or path",
    )
    args = ap.parse_args()

    cache = Path(args.cache)
    overrides = parse_overrides(args.local)
    items = []
    for spec in CATALOG:
        try:
            items.append(build_item(spec, cache, overrides))
        except Exception as exc:
            print(f"error {spec['id']}: {exc}", file=sys.stderr)
            return 1

    doc = {"source": HOSTED, "items": items}
    out = Path(args.out)
    out.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
