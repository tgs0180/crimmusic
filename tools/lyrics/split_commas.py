import argparse, pathlib, re, shutil, sys, time

sys.path.insert(0, str(pathlib.Path(__file__).parent))

from align import align

TIMESTAMP = re.compile(r"\[(\d+):(\d+(?:\.\d+)?)\]")
MIN_GAP = 0.6


def fragments(text):
    parts, current, depth, quoted = [], "", 0, False
    i = 0
    while i < len(text):
        c = text[i]
        if c in "\"“”":
            quoted = not quoted
        elif c in "([":
            depth += 1
        elif c in ")]" and depth:
            depth -= 1
        if c == "," and depth == 0 and not quoted and i + 1 < len(text) and text[i + 1] == " ":
            parts.append(current.strip())
            current = ""
            i += 2
            continue
        current += c
        i += 1
    parts.append(current.strip())
    parts = [p for p in parts if p]

    merged = []
    for p in parts:
        if merged and len(merged[-1].split()) < 2:
            merged[-1] = merged[-1] + ", " + p
        else:
            merged.append(p)
    if len(merged) > 1 and len(merged[-1].split()) < 2:
        last = merged.pop()
        merged[-1] = merged[-1] + ", " + last
    return merged


def parse(text):
    header, lines = [], []
    for raw in text.splitlines():
        stamps = TIMESTAMP.findall(raw)
        if not stamps or not raw.lstrip().startswith("["):
            if raw.strip():
                header.append(raw.strip())
            continue
        body = TIMESTAMP.sub("", raw).strip()
        for m, s in stamps:
            lines.append((int(m) * 60 + float(s), body))
    lines.sort(key=lambda x: x[0])
    return header, lines


def stamp(t):
    m, s = divmod(max(0.0, t), 60)
    return f"[{int(m):02d}:{s:05.2f}]"


def split_lrc(text, mp3, duration=None):
    header, lines = parse(text)
    pieces = [fragments(body) if body else [""] for _, body in lines]
    if all(len(p) <= 1 for p in pieces):
        return text, 0

    flat = [f for p in pieces for f in p]
    owner = [i for i, p in enumerate(pieces) for _ in p]
    aligned = None
    try:
        res = align(mp3, "\n".join(f if f else "-" for f in flat), filter_lines=False)
        if len(res["starts"]) == len(flat):
            aligned = res
    except Exception:
        aligned = None

    out = []
    split_count = 0
    k = 0
    for i, (start, body) in enumerate(lines):
        parts = pieces[i]
        if len(parts) <= 1:
            out.append((start, body))
            k += len(parts)
            continue
        end = lines[i + 1][0] if i + 1 < len(lines) else min(duration or start + 4.0, start + 0.08 * len(body) + 1.0)
        total_chars = sum(len(p) for p in parts)
        times = [start]
        chars_before = 0
        for j in range(1, len(parts)):
            chars_before += len(parts[j - 1])
            estimate = start + (end - start) * chars_before / max(1, total_chars)
            t = estimate
            if aligned is not None and aligned["matched_lines"][k + j] >= 0.5:
                candidate = aligned["starts"][k + j]
                if times[-1] + MIN_GAP <= candidate <= end - MIN_GAP:
                    t = candidate
            times.append(t)
        ok = all(b - a >= MIN_GAP for a, b in zip(times, times[1:])) and end - times[-1] >= MIN_GAP
        if ok:
            out.extend(zip(times, parts))
            split_count += 1
        else:
            out.append((start, body))
        k += len(parts)

    lines_out = header + [f"{stamp(t)} {x}".rstrip() for t, x in out]
    return "\n".join(lines_out) + "\n", split_count


def main():
    from make_lrc import tags
    ap = argparse.ArgumentParser()
    ap.add_argument("folder")
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()
    folder = pathlib.Path(args.folder).expanduser()
    backup = pathlib.Path.home() / ".cache/crimmusic/lrc-backup" / ("before-comma-split-" + time.strftime("%Y%m%d-%H%M%S"))
    files = changed = splits = 0
    for lrc in sorted(folder.glob("*.lrc")):
        mp3 = lrc.with_suffix(".mp3")
        if not mp3.exists():
            continue
        files += 1
        text = lrc.read_text(encoding="utf-8")
        new, count = split_lrc(text, mp3, tags(mp3)[2])
        if count == 0:
            continue
        changed += 1
        splits += count
        print(f"{count:3} lines split  {lrc.name}", flush=True)
        if not args.dry_run:
            backup.mkdir(parents=True, exist_ok=True)
            shutil.copy2(lrc, backup / lrc.name)
            lrc.write_text(new, encoding="utf-8")
    print(f"{files} files, {changed} changed, {splits} lines split" + (" (dry run)" if args.dry_run else f", backups in {backup}"))


if __name__ == "__main__":
    main()
