import argparse, json, pathlib, shutil, sys, time

sys.path.insert(0, str(pathlib.Path(__file__).parent))

from make_lrc import NOTHING, TEMPORARY, WRITTEN, decide, tags
from split_commas import split_lrc

ap = argparse.ArgumentParser()
ap.add_argument("folder")
ap.add_argument("--inventory")
ap.add_argument("--candidates")
ap.add_argument("--report", default=str(pathlib.Path.home() / ".cache/crimmusic/lyrics-report.json"))
args = ap.parse_args()

inventory = {}
if args.inventory:
    for e in json.load(open(args.inventory)):
        status = "found" if (e.get("synced") or e.get("plain")) else ("failed" if e["how"] == "failed" else "none")
        entry = {"syncedLyrics": e.get("synced"), "plainLyrics": e.get("plain"), "id": e.get("lrclib_id")}
        inventory[e["file"]] = (status, entry)
candidates = json.load(open(args.candidates)) if args.candidates else {}

backup = pathlib.Path.home() / ".cache/crimmusic/lrc-backup" / time.strftime("%Y%m%d-%H%M%S")
report = {}
songs = sorted(pathlib.Path(args.folder).expanduser().glob("*.mp3"))
for i, mp3 in enumerate(songs):
    lrc_path = mp3.with_suffix(".lrc")
    if lrc_path.exists():
        backup.mkdir(parents=True, exist_ok=True)
        shutil.copy2(lrc_path, backup / lrc_path.name)
    artist, title, duration = tags(mp3)
    notes = []
    code, lrc = decide(mp3, artist, title, duration, inventory.get(str(mp3)), candidates.get(str(mp3)), log=notes.append)
    source = None
    if code == WRITTEN:
        lrc, _ = split_lrc(lrc, mp3, duration)
        lrc_path.write_text(lrc)
        source = next((l[8:-1] for l in lrc.splitlines() if l.startswith("[source:")), None)
    report[str(mp3)] = {"code": code, "source": source, "notes": notes}
    print(f"{i + 1}/{len(songs)} {['written', '', 'nothing', 'temporary failure'][code] if code in (0, 2, 3) else code:18} {source or '':45} {mp3.name}", flush=True)
json.dump(report, open(args.report, "w"), indent=1, ensure_ascii=False)
print("BATCH DONE", flush=True)
