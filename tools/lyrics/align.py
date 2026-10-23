import json, re, sys, pathlib, unicodedata
from difflib import SequenceMatcher

import os
TRANSCRIPTS = pathlib.Path(os.environ.get("CRIMMUSIC_TRANSCRIPTS", pathlib.Path.home() / ".cache/crimmusic/transcripts"))
TRANSCRIPTS.mkdir(parents=True, exist_ok=True)
_model = None

def model():
    global _model
    if _model is None:
        from faster_whisper import WhisperModel
        import os
        device = os.environ.get("ALIGN_DEVICE", "cpu")
        compute = os.environ.get("ALIGN_COMPUTE", "int8")
        _model = WhisperModel(os.environ.get("ALIGN_MODEL", "large-v3-turbo"), device=device, compute_type=compute, cpu_threads=10)
    return _model

def transcribe(mp3):
    mp3 = pathlib.Path(mp3)
    cache = TRANSCRIPTS / (mp3.stem + ".json")
    if cache.exists():
        return json.loads(cache.read_text())
    from audio import load
    segments, info = model().transcribe(load(mp3), word_timestamps=True, condition_on_previous_text=False, vad_filter=False)
    words = [{"w": w.word.strip(), "s": round(w.start, 2), "e": round(w.end, 2), "p": round(w.probability, 3)}
             for seg in segments for w in seg.words if w.word.strip()]
    data = {"language": info.language, "language_probability": info.language_probability, "duration": info.duration, "words": words}
    cache.write_text(json.dumps(data, ensure_ascii=False))
    return data

def transcript(mp3):
    mp3 = pathlib.Path(mp3)
    data = transcribe(mp3)
    if not data.get("gapfill"):
        data = fill_gaps(mp3, data)
        (TRANSCRIPTS / (mp3.stem + ".json")).write_text(json.dumps(data, ensure_ascii=False))
    return data

GAP_SECONDS = 3.0

def fill_gaps(mp3, data):
    if data.get("gapfill"):
        return data
    from audio import load
    audio = load(mp3)
    sr = 16000
    words = data["words"]
    duration = data["duration"]
    edges = [0.0] + [t for w in words for t in (w["s"], w["e"])] + [duration]
    gaps = []
    for i in range(0, len(edges) - 1, 2):
        start, end = edges[i], edges[i + 1]
        if end - start >= GAP_SECONDS:
            gaps.append((start, end))
    added = []
    for start, end in gaps:
        a = max(0.0, start - 0.3)
        b = min(duration, end + 0.3)
        clip = audio[int(a * sr):int(b * sr)]
        if len(clip) < sr:
            continue
        segments, _ = model().transcribe(clip, word_timestamps=True, condition_on_previous_text=False, vad_filter=False,
                                         language=data["language"])
        for seg in segments:
            for w in seg.words:
                ws, we = w.start + a, w.end + a
                if w.word.strip() and w.probability >= 0.4 and ws >= start - 0.05 and we <= end + 0.05:
                    added.append({"w": w.word.strip(), "s": round(ws, 2), "e": round(we, 2), "p": round(w.probability, 3), "gap": True})
    data["words"] = sorted(words + added, key=lambda w: w["s"])
    data["gapfill"] = True
    data["gaps"] = len(gaps)
    data["gap_words"] = len(added)
    return data

def norm(word):
    word = unicodedata.normalize("NFKD", word.lower())
    word = "".join(c for c in word if not unicodedata.combining(c))
    word = word.replace("’", "'").replace("‘", "'")
    word = re.sub(r"in'$", "ing", word)
    return re.sub(r"[^\w']", "", word).strip("'")

def tokens(text):
    return [t for t in (norm(w) for w in text.split()) if t]

GENIUS_JUNK = re.compile(r"^(\d*\s*embed|see .+ live|get tickets as low as .*|you might also like)$", re.I)

def lyric_lines(plain, filter_lines=True):
    lines = []
    skip_titles = 0
    for raw in plain.splitlines():
        line = raw.strip()
        if not line: continue
        if not filter_lines:
            lines.append(line)
            continue
        if line.lower() == "you might also like":
            skip_titles = 8
            continue
        if skip_titles:
            if len(line.split()) <= 4 and not line.startswith("("):
                skip_titles -= 1
                continue
            skip_titles = 0
        if GENIUS_JUNK.match(line): continue
        line = re.sub(r"\d*\s*Embed$", "", line).strip()
        if not line: continue
        if re.fullmatch(r"\[.*\]", line): continue
        if re.fullmatch(r"\(?\s*(prod(\.|uced)?|produced by|beat by|mixed by)\b.*", line, re.I): continue
        if line.startswith("**") or re.fullmatch(r"!!.*!!", line): continue
        if not re.search(r"[^\W\d_]", line): continue
        lines.append(line)
    return lines

def similarity(a, b):
    if a == b: return 1.0
    return SequenceMatcher(None, a, b, autojunk=False).ratio()

def align_tokens(lyr, asr):
    n, m = len(lyr), len(asr)
    GAP = -1.0
    score = [[0.0] * (m + 1) for _ in range(n + 1)]
    move = [[0] * (m + 1) for _ in range(n + 1)]
    for i in range(1, n + 1):
        score[i][0] = score[i - 1][0] + GAP; move[i][0] = 1
    for j in range(1, m + 1):
        score[0][j] = score[0][j - 1] + GAP * 0.5; move[0][j] = 2
    for i in range(1, n + 1):
        li = lyr[i - 1]
        row, prev = score[i], score[i - 1]
        for j in range(1, m + 1):
            sim = similarity(li, asr[j - 1])
            diag = prev[j - 1] + (2.0 if sim >= 0.8 else 1.0 if sim >= 0.6 else -1.0)
            up = prev[j] + GAP
            left = row[j - 1] + GAP * 0.5
            best = max(diag, up, left)
            row[j] = best
            move[i][j] = 0 if best == diag else 1 if best == up else 2
    pairs = {}
    i, j = n, m
    while i > 0 and j > 0:
        mv = move[i][j]
        if mv == 0:
            if similarity(lyr[i - 1], asr[j - 1]) >= 0.6: pairs[i - 1] = j - 1
            i -= 1; j -= 1
        elif mv == 1: i -= 1
        else: j -= 1
    return pairs

def align(mp3, plain, filter_lines=True):
    data = transcript(mp3)
    words = data["words"]
    lines = lyric_lines(plain, filter_lines)
    lyr, owner = [], []
    for li, line in enumerate(lines):
        for t in tokens(line):
            lyr.append(t); owner.append(li)
    asr = [norm(w["w"]) for w in words]
    pairs = align_tokens(lyr, asr)

    starts, matched_counts = [None] * len(lines), [0] * len(lines)
    totals = [0] * len(lines)
    for k, li in enumerate(owner):
        totals[li] += 1
        if k in pairs:
            matched_counts[li] += 1
            if starts[li] is None:
                first_tok = owner.index(li)
                offset = k - first_tok
                t = words[pairs[k]]["s"]
                starts[li] = t if offset == 0 else max(0.0, t - offset * 0.25)
    known = [i for i, s in enumerate(starts) if s is not None]
    duration = data["duration"]
    for i in range(len(lines)):
        if starts[i] is not None: continue
        before = max((k for k in known if k < i), default=None)
        after = min((k for k in known if k > i), default=None)
        if before is not None and after is not None:
            starts[i] = starts[before] + (starts[after] - starts[before]) * (i - before) / (after - before)
        elif before is not None:
            starts[i] = min(duration, starts[before] + 2.5 * (i - before))
        elif after is not None:
            starts[i] = max(0.0, starts[after] - 2.5 * (after - i))
        else:
            starts[i] = 0.0
    for i in range(1, len(starts)):
        if starts[i] < starts[i - 1]: starts[i] = starts[i - 1]
    line_words = [[] for _ in lines]
    for k, li in enumerate(owner):
        if k in pairs:
            line_words[li].append(pairs[k])
    matched_fraction = [matched_counts[i] / max(1, totals[i]) for i in range(len(lines))]
    dropped = set()
    if filter_lines:
        def key(text):
            return " ".join(tokens(text))
        strong = [i for i in range(len(lines)) if matched_fraction[i] >= 0.34]
        strong_keys = set(key(lines[i]) for i in strong)
        claimed = set(j for i in strong for j in line_words[i])
        i = 0
        while i < len(lines):
            if matched_fraction[i] >= 0.34:
                i += 1
                continue
            j = i
            while j < len(lines) and matched_fraction[j] < 0.34:
                j += 1
            prev = max((k for k in strong if k < i), default=None)
            nxt = min((k for k in strong if k >= j), default=None)
            lo = words[max(line_words[prev])]["e"] if prev is not None and line_words[prev] else 0.0
            hi = words[min(line_words[nxt])]["s"] if nxt is not None and line_words[nxt] else duration
            repeats = [k for k in range(i, j) if key(lines[k]) in strong_keys]
            if len(repeats) >= 2:
                foreign = sum(1 for idx, w in enumerate(words) if lo < w["s"] < hi and idx not in claimed and w["p"] >= 0.6)
                if foreign >= 3 * (j - i):
                    dropped.update(repeats)
            if (hi - lo) < 0.8 * (j - i):
                for k in range(i + 1, j):
                    if k not in dropped and key(lines[k]) == key(lines[k - 1]):
                        dropped.add(k)
            i = j
    ratio = len(pairs) / max(1, len(lyr))
    coverage = len(set(pairs.values())) / max(1, len(asr))
    line_ratio = sum(1 for i in range(len(lines)) if totals[i] and matched_counts[i] / totals[i] >= 0.5) / max(1, len(lines))
    keep = [i for i in range(len(lines)) if i not in dropped]
    return {"lines": [lines[i] for i in keep], "starts": [starts[i] for i in keep], "word_ratio": ratio, "coverage": coverage, "line_ratio": line_ratio,
            "matched_lines": [matched_fraction[i] for i in keep], "dropped": [lines[i] for i in sorted(dropped)],
            "language": data["language"], "asr_words": len(words), "lyric_words": len(lyr)}

def to_lrc(result, header):
    out = [f"[{k}:{v}]" for k, v in header.items()]
    for t, line in zip(result["starts"], result["lines"]):
        m, s = divmod(t, 60)
        out.append(f"[{int(m):02d}:{s:05.2f}] {line}")
    return "\n".join(out) + "\n"
