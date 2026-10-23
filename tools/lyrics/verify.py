import re


def parse_lrc(text):
    out = []
    for line in text.splitlines():
        m = re.match(r"\[(\d+):(\d+(?:\.\d+)?)\](.*)", line)
        if m and m.group(3).strip():
            out.append((int(m.group(1)) * 60 + float(m.group(2)), m.group(3).strip()))
    return out


def text_ok(result):
    return result["word_ratio"] + result["coverage"] >= 0.75 and result["coverage"] >= 0.40


def timing_fraction(truth, result, shift=0.0):
    anchored = [(t + shift, s) for (t, _), s, m in zip(truth, result["starts"], result["matched_lines"]) if m >= 0.5]
    if len(anchored) < 5:
        return None, anchored
    return sum(abs(t - s) <= 1.5 for t, s in anchored) / len(anchored), anchored
