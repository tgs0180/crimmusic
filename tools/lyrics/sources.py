import json, re, subprocess, time, urllib.parse, urllib.request

UA = "crimmusic (https://github.com/tgs0180/crimmusic)"
JUNK = re.compile(r"(https?://|www\.|@|#|subscribe|follow|instagram|twitter|tiktok|spotify|apple music|soundcloud|youtube|"
                  r"stream|download|copyright|all rights|no copyright|fair use|prod\.? by|produced by|mixed by|mastered|"
                  r"video by|directed|shot by|edited by|lyrics:|lyric video|turn on|notifications|out now|available|"
                  r"booking|business|inquir|contact|email|merch|tickets|\bdm\b|\bfeat\b\.?\s*$)", re.I)

def lyric_block(text):
    if not text: return []
    blocks, cur = [], []
    for raw in text.splitlines():
        line = raw.strip()
        if not line:
            cur.append(""); continue
        if JUNK.search(line) or len(line) > 90:
            if cur: blocks.append(cur); cur = []
            continue
        cur.append(line)
    if cur: blocks.append(cur)
    def size(b): return sum(1 for l in b if l and not re.fullmatch(r"[\[(].*[\])]", l))
    good = [b for b in blocks if size(b) >= 4]
    out = ["\n".join(b).strip() for b in good if size(b) >= 8]
    if len(good) > 1:
        out.append("\n".join("\n".join(b) for b in good).strip())
    return out

def ytdlp_json(query, count):
    cmd = ["yt-dlp", "--skip-download", "--no-warnings", "--ignore-errors",
           "--print", "%(.{id,title,channel,uploader,duration,description,webpage_url})j", f"{query}"]
    out = subprocess.run(cmd, capture_output=True, text=True, timeout=180).stdout
    items = []
    for line in out.splitlines():
        try: items.append(json.loads(line))
        except Exception: pass
    return items[:count]

def youtube_descriptions(artist, title, count=5):
    found = []
    for item in ytdlp_json(f"ytsearch{count}:{artist} {title} lyrics", count):
        for block in lyric_block(item.get("description") or ""):
            found.append({"source": "youtube-description", "url": item.get("webpage_url"), "video": item.get("title"),
                          "duration": item.get("duration"), "text": block})
    return found

def soundcloud_descriptions(artist, title, count=3):
    found = []
    for item in ytdlp_json(f"scsearch{count}:{artist} {title}", count):
        for block in lyric_block(item.get("description") or ""):
            found.append({"source": "soundcloud-description", "url": item.get("webpage_url"), "video": item.get("title"),
                          "duration": item.get("duration"), "text": block})
    return found

def _get_json(url, headers=None):
    req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0", **(headers or {})})
    with urllib.request.urlopen(req, timeout=15) as r:
        return json.loads(r.read())

def netease(artist, title, duration):
    found = []
    try:
        res = _get_json("https://music.163.com/api/search/get?" + urllib.parse.urlencode({"s": f"{artist} {title}", "type": 1, "limit": 8}),
                        {"Referer": "https://music.163.com"})
        songs = ((res or {}).get("result") or {}).get("songs") or []
    except Exception:
        return found
    want_a, want_t = artist.lower(), title.lower()
    for s in songs:
        artists = " ".join(a.get("name", "") for a in s.get("artists", [])).lower()
        name = (s.get("name") or "").lower()
        if not artists or not (want_a in artists or any(a.get("name", "").lower() in want_a for a in s.get("artists", []) if a.get("name"))):
            continue
        if not (want_t in name or name in want_t):
            continue
        if duration and s.get("duration") and abs(s["duration"] / 1000 - duration) > 5:
            continue
        try:
            lyr = _get_json(f"https://music.163.com/api/song/lyric?id={s['id']}&lv=1&kv=1&tv=-1", {"Referer": "https://music.163.com"})
        except Exception:
            continue
        lrc = ((lyr or {}).get("lrc") or {}).get("lyric") or ""
        if lrc.strip():
            found.append({"source": "netease", "url": f"https://music.163.com/song?id={s['id']}", "video": s.get("name"), "lrc": lrc})
        time.sleep(0.5)
    return found
