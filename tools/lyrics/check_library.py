import argparse, collections, json, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))

from align import align, transcript
from make_lrc import tags
from verify import parse_lrc, text_ok, timing_fraction
from transcribed import HALLUCINATION_MARKERS

ap = argparse.ArgumentParser()
ap.add_argument("folder")
ap.add_argument("--json")
args = ap.parse_args()

folder = pathlib.Path(args.folder).expanduser()
songs = sorted(folder.glob("*.mp3"))
problems, rows = [], []
for mp3 in songs:
    lrc_path = mp3.with_suffix(".lrc")
    if not lrc_path.exists():
        rows.append({"song": mp3.name, "status": "no lrc"})
        continue
    text = lrc_path.read_text()
    source = next((l[8:-1] for l in text.splitlines() if l.startswith("[source:")), "unknown")
    lines = parse_lrc(text)
    _, _, duration = tags(mp3)
    issues = []
    if len(lines) < 4:
        issues.append(f"only {len(lines)} lines")
    times = [t for t, _ in lines]
    if any(b < a for a, b in zip(times, times[1:])):
        issues.append("timestamps out of order")
    if times and times[-1] > duration + 1:
        issues.append(f"last line at {times[-1]:.1f}s but song is {duration:.1f}s")
    row = {"song": mp3.name, "source": source, "lines": len(lines)}
    if lines:
        res = align(mp3, "\n".join(x for _, x in lines), filter_lines=False)
        row.update({"words": round(res["word_ratio"], 2), "coverage": round(res["coverage"], 2)})
        if source == "transcribed":
            counts = collections.Counter(x.lower() for _, x in lines)
            top = counts.most_common(1)[0][1]
            if top > max(6, len(lines) * 0.4):
                issues.append(f"one line repeated {top} times (possible hallucination loop)")
            suspicious = [x for _, x in lines if HALLUCINATION_MARKERS.search(x)]
            if suspicious:
                issues.append(f"possible hallucinations: {suspicious[:3]}")
            if res["word_ratio"] < 0.8:
                issues.append(f"transcribed text only {res['word_ratio']:.0%} found in transcript")
        else:
            if not text_ok(res):
                issues.append(f"text does not match audio (words {res['word_ratio']:.2f}, coverage {res['coverage']:.2f})")
            frac, _ = timing_fraction(lines, res)
            row["timing"] = None if frac is None else round(frac, 2)
            if frac is not None and frac < 0.75:
                issues.append(f"only {frac:.0%} of lines timed within 1.5s of where they are heard")
    row["issues"] = issues
    row["status"] = "problem" if issues else "ok"
    rows.append(row)
    if issues:
        problems.append((mp3.name, issues))

orphans = [p.name for p in folder.glob("*.lrc") if not p.with_suffix(".mp3").exists()]
by_status = collections.Counter(r["status"] for r in rows)
by_source = collections.Counter(re.sub(r"[ ,].*", "", r.get("source", "")) for r in rows if r.get("source"))
print(f"{len(songs)} songs: {dict(by_status)}")
print(f"sources: {dict(by_source)}")
print(f"lrc files without a song: {orphans or 'none'}")
for name, issues in problems:
    print(f"PROBLEM {name}: {'; '.join(issues)}")
if args.json:
    json.dump(rows, open(args.json, "w"), indent=1, ensure_ascii=False)
