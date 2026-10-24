import argparse, json, pathlib, re, statistics, subprocess, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))

import lrclib
import sources
from align import align, to_lrc, transcript
from transcribed import assess
from verify import parse_lrc, text_ok, timing_fraction
from split_commas import split_lrc

CREDIT = re.compile(r"^\s*(作词|作曲|编曲|制作人|混音|母带|和声|吉他|贝斯|鼓|录音|监制|出品|OP|SP|Lyricist|Composer|Producer|Arranger)\s*[:：]", re.I)
PRIORITY = {"lrclib-plain": 0, "netease": 1, "youtube-description": 2, "soundcloud-description": 3}
WRITTEN, NOTHING, TEMPORARY = 0, 2, 3


def tags(mp3):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration:format_tags=title,artist", "-of", "json", str(mp3)],
                         capture_output=True, text=True).stdout
    fmt = json.loads(out)["format"]
    t = {k.lower(): v for k, v in fmt.get("tags", {}).items()}
    return t.get("artist", ""), t.get("title", mp3.stem), float(fmt.get("duration", 0))


def header(artist, title, source):
    return {"ar": artist, "ti": title, "source": source, "re": "crimmusic lyrics tool"}


def shifted_lrc(synced, shift):
    out = []
    for line in synced.splitlines():
        m = re.match(r"\[(\d+):(\d+(?:\.\d+)?)\](.*)", line)
        if not m:
            continue
        t = max(0.0, int(m.group(1)) * 60 + float(m.group(2)) + shift)
        mm, ss = divmod(t, 60)
        out.append(f"[{int(mm):02d}:{ss:05.2f}]{m.group(3)}")
    return "\n".join(out)


def check_synced(mp3, synced):
    truth = parse_lrc(synced)
    if len(truth) < 4:
        return None, None, None
    res = align(mp3, "\n".join(t for _, t in truth), filter_lines=False)
    if not text_ok(res):
        return "wrong-song", res, None
    frac, anchored = timing_fraction(truth, res)
    if frac is None or frac >= 0.75:
        return "keep", res, 0.0
    shift = statistics.median(s - t for t, s in anchored)
    frac2, _ = timing_fraction(truth, res, shift)
    if frac2 is not None and frac2 >= 0.75:
        return "shift", res, shift
    return "retime", res, None


def score_candidates(mp3, items):
    scored = []
    for c in items:
        if c["source"] == "netease":
            synced = [(t, x) for t, x in parse_lrc(c["lrc"]) if not CREDIT.match(x)]
            if len(synced) < 4:
                continue
            text = "\n".join(x for _, x in synced)
        else:
            synced, text = None, c["text"]
        res = align(mp3, text)
        scored.append({"source": c["source"], "url": c.get("url"), "score": res["word_ratio"] + res["coverage"],
                       "ok": text_ok(res), "synced": synced, "res": res})
    passing = [s for s in scored if s["ok"]]
    passing.sort(key=lambda s: (-round(s["score"], 1), PRIORITY.get(s["source"], 9)))
    return passing, scored


def decide(mp3, artist, title, duration, lrclib_result=None, candidates=None, log=print):
    if lrclib_result is None:
        status, entry = lrclib.lookup(artist, title, duration)
    else:
        status, entry = lrclib_result
    network_failed = status == "failed"
    entry = entry or {}

    if entry.get("syncedLyrics"):
        verdict, res, shift = check_synced(mp3, entry["syncedLyrics"])
        log(f"lrclib synced: {verdict}")
        if verdict == "keep":
            return WRITTEN, "\n".join(f"[{k}:{v}]" for k, v in header(artist, title, "lrclib").items()) + "\n" + entry["syncedLyrics"].strip() + "\n"
        if verdict == "shift":
            return WRITTEN, "\n".join(f"[{k}:{v}]" for k, v in header(artist, title, f"lrclib shifted {shift:+.2f}s").items()) + "\n" + shifted_lrc(entry["syncedLyrics"], shift) + "\n"
        if verdict == "retime":
            plain = "\n".join(t for _, t in parse_lrc(entry["syncedLyrics"]))
            res = align(mp3, plain)
            return WRITTEN, to_lrc(res, header(artist, title, "lrclib text, retimed to this file"))

    if candidates is None:
        candidates = []
        for fn in (lambda: sources.netease(artist, title, duration),
                   lambda: sources.youtube_descriptions(artist, title),
                   lambda: sources.soundcloud_descriptions(artist, title)):
            try:
                candidates += fn()
            except Exception as e:
                log(f"source error: {e}")
    candidates = list(candidates)
    if entry.get("plainLyrics") and not any(c["source"] == "lrclib-plain" for c in candidates):
        candidates.append({"source": "lrclib-plain", "url": f"https://lrclib.net/api/get/{entry.get('id')}", "text": entry["plainLyrics"]})

    passing, scored = score_candidates(mp3, candidates)
    log(f"candidates: {len(scored)} scored, {len(passing)} passed" + (f", best {passing[0]['source']} {passing[0]['score']:.2f}" if passing else ""))
    if passing:
        best = passing[0]
        if best["synced"]:
            frac, _ = timing_fraction(best["synced"], best["res"])
            if frac is not None and frac >= 0.75:
                lines = "\n".join(f"[{int(t // 60):02d}:{t % 60:05.2f}] {x}" for t, x in best["synced"])
                return WRITTEN, "\n".join(f"[{k}:{v}]" for k, v in header(artist, title, best["source"]).items()) + "\n" + lines + "\n"
        return WRITTEN, to_lrc(best["res"], header(artist, title, f"{best['source']}, aligned to this file"))

    if network_failed:
        return TEMPORARY, None

    lines, why = assess(transcript(mp3))
    log(f"transcription: {why}")
    if lines:
        out = [f"[{k}:{v}]" for k, v in header(artist, title, "transcribed").items()]
        out += [f"[{int(t // 60):02d}:{t % 60:05.2f}] {x}" for t, x in lines]
        return WRITTEN, "\n".join(out) + "\n"
    return NOTHING, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mp3")
    ap.add_argument("--artist")
    ap.add_argument("--title")
    args = ap.parse_args()
    mp3 = pathlib.Path(args.mp3)
    artist, title, duration = tags(mp3)
    artist = args.artist or artist
    title = args.title or title
    code, lrc = decide(mp3, artist, title, duration, log=lambda m: print(f"[lyrics] {mp3.name}: {m}", flush=True))
    if code == WRITTEN and not mp3.with_suffix(".lrc").exists():
        lrc, _ = split_lrc(lrc, mp3, duration)
        mp3.with_suffix(".lrc").write_text(lrc)
    sys.exit(code)


if __name__ == "__main__":
    main()
