import argparse, json, pathlib, re, sys

sys.path.insert(0, str(pathlib.Path(__file__).parent))

from align import align, to_lrc
from make_lrc import tags
from verify import text_ok
from split_commas import split_lrc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("mp3")
    ap.add_argument("lyrics")
    ap.add_argument("out_lrc")
    ap.add_argument("result_json")
    args = ap.parse_args()

    result = {"ok": False, "matches": False, "message": ""}
    try:
        mp3 = pathlib.Path(args.mp3)
        text = pathlib.Path(args.lyrics).read_text(encoding="utf-8", errors="replace")
        text = "\n".join(re.sub(r"^\s*(\[\d+:\d+(?:\.\d+)?\]\s*)+", "", line) for line in text.splitlines())
        try:
            readable = mp3.is_file() and tags(mp3)[2] > 0
        except Exception:
            readable = False
        if not readable:
            result["message"] = f"Could not read the song file: {mp3.name}"
        elif not text.strip():
            result["message"] = "The lyrics are empty."
        else:
            artist, title, _ = tags(mp3)
            res = align(mp3, text)
            if not res["lines"]:
                result["message"] = "No lyric lines found in the text."
            else:
                header = {"ar": artist, "ti": title, "source": "manual lyrics, aligned to this file", "re": "crimmusic make lyrics"}
                lrc_text, _ = split_lrc(to_lrc(res, header), mp3)
                pathlib.Path(args.out_lrc).write_text(lrc_text, encoding="utf-8")
                weak = sum(1 for m in res["matched_lines"] if m < 0.34)
                result.update({
                    "ok": True,
                    "matches": text_ok(res),
                    "score": round(res["word_ratio"] + res["coverage"], 2),
                    "words": round(res["word_ratio"], 2),
                    "coverage": round(res["coverage"], 2),
                    "lines": len(res["lines"]),
                    "weak_lines": weak,
                    "dropped": res["dropped"],
                })
    except Exception as e:
        result["message"] = f"{type(e).__name__}: {e}"
    pathlib.Path(args.result_json).write_text(json.dumps(result, ensure_ascii=False), encoding="utf-8")
    sys.exit(0 if result["ok"] else 1)


if __name__ == "__main__":
    main()
