#!/bin/sh
set -e
BASE="$HOME/.local/share/crimmusic"
mkdir -p "$BASE"
[ -x "$BASE/bootstrap/bin/uv" ] || { python3 -m venv "$BASE/bootstrap" && "$BASE/bootstrap/bin/pip" install -q uv; }
[ -x "$BASE/venv/bin/python" ] || "$BASE/bootstrap/bin/uv" venv -q --python 3.12 "$BASE/venv"
"$BASE/bootstrap/bin/uv" pip install -q --python "$BASE/venv/bin/python" faster-whisper
if command -v nvidia-smi >/dev/null 2>&1; then
    "$BASE/bootstrap/bin/uv" pip install -q --python "$BASE/venv/bin/python" nvidia-cublas-cu12 "nvidia-cudnn-cu12==9.*"
fi
echo "lyrics tool ready"
