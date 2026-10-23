import json, time, urllib.error, urllib.parse, urllib.request

UA = "crimmusic (https://github.com/tgs0180/crimmusic)"


def clean_title(title, artist):
    out, depth = "", 0
    for c in title:
        if c in "([":
            depth += 1
            continue
        if c in ")]" and depth:
            depth -= 1
            continue
        if not depth:
            out += c
    low = out.lower()
    for cut in (" prod", " ft.", " feat"):
        p = low.find(cut)
        if p != -1:
            out, low = out[:p], low[:p]
    pre = artist.lower() + " - "
    if low.startswith(pre):
        out = out[len(pre):]
    return out.strip().rstrip("- ").strip()


def _get(url):
    for attempt in range(4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=15) as r:
                return 200, json.loads(r.read())
        except urllib.error.HTTPError as e:
            if e.code == 404:
                return 404, None
            time.sleep(2 + attempt * 3)
        except Exception:
            time.sleep(2 + attempt * 3)
    return 0, None


def lookup(artist, title, duration):
    cleaned = clean_title(title, artist)
    titles = [title] + ([cleaned] if cleaned and cleaned != title else [])
    failed = False
    for t in titles:
        code, d = _get("https://lrclib.net/api/get?" + urllib.parse.urlencode({"artist_name": artist, "track_name": t}))
        if code == 200 and d:
            return "found", d
        if code == 0:
            failed = True
    want_t, want_a = (cleaned or title).lower(), artist.lower()
    code, res = _get("https://lrclib.net/api/search?" + urllib.parse.urlencode({"q": artist + " " + (cleaned or title)}))
    if code == 200 and isinstance(res, list):
        cands = []
        for e in res:
            fa, ft = (e.get("artistName") or "").lower(), (e.get("trackName") or "").lower()
            if fa and ft and (want_a in fa or fa in want_a) and (want_t in ft or ft in want_t):
                cands.append(e)
        cands.sort(key=lambda e: (not e.get("syncedLyrics"), not e.get("plainLyrics"), abs((e.get("duration") or 0) - duration)))
        if cands:
            return "found", cands[0]
    elif code == 0:
        failed = True
    return ("failed" if failed else "none"), None
