import re, statistics

HALLUCINATION_MARKERS = re.compile(
    r"(thank(s)?( you)? for (watching|listening)|subtitles? by|amara\.?org|subscribe|wgbh|production of|"
    r"see you next time|see you in the next|transcribed by|captioning|caption(s|ed)? by|^thank you\.?$|^you\.?$)", re.I)
MIN_WORDS = 30
MIN_MEAN_PROB = 0.5


def chunks(words, pause=0.5):
    out, cur = [], []
    for w in words:
        if cur and w["s"] - cur[-1]["e"] >= pause:
            out.append(cur)
            cur = []
        cur.append(w)
    if cur:
        out.append(cur)
    return out


def clean_words(words):
    words = [w for w in words if not w.get("gap")]
    kept = []
    for chunk in chunks(words):
        text = " ".join(w["w"] for w in chunk).strip()
        if len(chunk) <= 12 and HALLUCINATION_MARKERS.search(text):
            continue
        tail = [w["w"].lower().strip(".,!?") for w in chunk[-2:]]
        if tail == ["thank", "you"] and chunk[-1]["w"].endswith("."):
            chunk = chunk[:-2]
        kept += chunk
    return [w for w in kept if not (w["e"] - w["s"] <= 0.0 and w["p"] < 0.5) and w["p"] >= 0.15]


def build_lines(words, pause=0.6, max_words=12):
    lines, cur = [], []
    for w in words:
        if cur:
            gap = w["s"] - cur[-1]["e"]
            ends = cur[-1]["w"].endswith((".", "?", "!"))
            starts_sentence = (w["w"][:1].isupper() and not re.match(r"I('|$|’)", w["w"])
                               and not cur[-1]["w"].endswith(",") and len(cur) >= 5)
            if gap >= pause or ends or starts_sentence or len(cur) >= max_words:
                lines.append(cur); cur = []
        cur.append(w)
    if cur: lines.append(cur)
    out = []
    for ln in lines:
        if len(ln) <= 2 and all(w["p"] < 0.5 for w in ln):
            continue
        text = " ".join(w["w"] for w in ln).strip()
        text = re.sub(r"\s+([,.?!])", r"\1", text).rstrip(",")
        text = re.sub(r"(\w) -(\w)", r"\1-\2", text)
        if not re.search(r"[^\W\d_]", text): continue
        out.append((ln[0]["s"], text[0].upper() + text[1:]))
    return out

def assess(data):
    words = clean_words(data["words"])
    if len(words) < MIN_WORDS:
        return None, f"only {len(words)} confident words"
    mean_p = statistics.mean(w["p"] for w in words)
    if mean_p < MIN_MEAN_PROB:
        return None, f"low confidence ({mean_p:.2f})"
    return build_lines(words), f"{len(words)} words, confidence {mean_p:.2f}"
