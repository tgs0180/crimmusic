#!/bin/sh
VENV="$HOME/.local/share/crimmusic/venv"
PY="$VENV/bin/python"
[ -x "$PY" ] || exit 1
SITE=$("$PY" -c "import site; print(site.getsitepackages()[0])")
if [ -d "$SITE/nvidia/cublas/lib" ] && command -v nvidia-smi >/dev/null 2>&1; then
    export LD_LIBRARY_PATH="$SITE/nvidia/cublas/lib:$SITE/nvidia/cudnn/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export ALIGN_DEVICE="${ALIGN_DEVICE:-cuda}"
else
    export ALIGN_DEVICE="${ALIGN_DEVICE:-cpu}"
fi
export ALIGN_COMPUTE="${ALIGN_COMPUTE:-int8}"
exec "$PY" "$@"
