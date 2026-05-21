#!/bin/sh
set -eu

script_dir="$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)"
repo_root="$(CDPATH= cd -- "$script_dir/.." && pwd)"
src_dir="$repo_root/src"
nnue_dir="$src_dir/nnue"

# Auto-detect evaluate.h location
if [ -f "$nnue_dir/evaluate.h" ]; then
  evaluate_file="$nnue_dir/evaluate.h"
elif [ -f "$src_dir/evaluate.h" ]; then
  evaluate_file="$src_dir/evaluate.h"
else
  >&2 echo "Error: cannot find evaluate.h in $nnue_dir/evaluate.h or $src_dir/evaluate.h"
  exit 1
fi

mkdir -p "$nnue_dir"

# Downloader
if command -v wget >/dev/null 2>&1; then
  download_cmd="wget -qO-"
elif command -v curl >/dev/null 2>&1; then
  download_cmd="curl -skL"
else
  >&2 printf "%s\n" \
    "Neither wget nor curl is installed." \
    "Install one of these tools to download NNUE files automatically."
  exit 1
fi

sha256_first12() {
  if command -v shasum >/dev/null 2>&1; then
    shasum -a 256 "$1" | awk '{print $1}' | cut -c 1-12
  elif command -v sha256sum >/dev/null 2>&1; then
    sha256sum "$1" | awk '{print $1}' | cut -c 1-12
  else
    >&2 echo "Error: sha256sum or shasum -a 256 is required to validate NNUE files."
    exit 1
  fi
}

file_size_bytes() {
  wc -c < "$1" | tr -d ' '
}

# Create src/nn-*.nnue as a real file when symlink is unusable (common on Windows/MSYS2).
# 1) Try ln -sf
# 2) Verify that resulting path has the same byte size as target
# 3) If not, fall back to cp -f (real file)
link_or_copy() {
  target="$1"
  link="$2"

  rm -f "$link" 2>/dev/null || true

  if ln -sf "$target" "$link" 2>/dev/null; then
    if [ -f "$link" ]; then
      tsz="$(file_size_bytes "$target" 2>/dev/null || echo 0)"
      lsz="$(file_size_bytes "$link" 2>/dev/null || echo -1)"
      if [ "$lsz" = "$tsz" ]; then
        return 0
      fi
    fi
  fi

  # Fallback: ensure a real file exists at src/nn-*.nnue
  cp -f "$target" "$link"
  echo "Note: created real NNUE file in src/ (symlink unusable): $(basename "$link")"
  return 0
}

get_nnue_filename() {
  # Extract nn-<12hex>.nnue from the macro line in evaluate.h
  grep -E "^#define[[:space:]]+$1[[:space:]]+\"[^\"]+\"" "$evaluate_file" \
    | sed -n 's/.*"\(nn-[a-z0-9]\{12\}\.nnue\)".*/\1/p'
}

validate_network() {
  # $1 full path, $2 min size
  fpath="$1"
  minsz="$2"
  [ -f "$fpath" ] || return 1
  fname="$(basename "$fpath")"
  sz="$(file_size_bytes "$fpath")"
  if [ "$sz" -lt "$minsz" ]; then
    rm -f "$fpath"
    return 1
  fi
  h12="$(sha256_first12 "$fpath")"
  if [ "$fname" != "nn-$h12.nnue" ]; then
    rm -f "$fpath"
    return 1
  fi
  return 0
}

fetch_network() {
  # $1 macro name, $2 min size
  macro="$1"
  minsz="$2"
  fname="$(get_nnue_filename "$macro")"
  if [ -z "$fname" ]; then
    >&2 echo "NNUE file name not found for: $macro (in $evaluate_file)"
    exit 1
  fi

  target="$nnue_dir/$fname"
  link="$src_dir/$fname"

  if [ -f "$target" ]; then
    if validate_network "$target" "$minsz"; then
      link_or_copy "$target" "$link"
      echo "Existing $fname validated, skipping download"
      return 0
    else
      echo "Removing invalid NNUE file: $fname"
      rm -f "$target"
    fi
  fi

  for url in \
    "https://tests.stockfishchess.org/api/nn/$fname" \
    "https://github.com/official-stockfish/networks/raw/master/$fname"
  do
    echo "Downloading from $url ..."
    if $download_cmd "$url" > "$target"; then
      if validate_network "$target" "$minsz"; then
        link_or_copy "$target" "$link"
        echo "Successfully validated $fname"
        return 0
      else
        echo "Downloaded $fname is invalid"
        rm -f "$target"
        continue
      fi
    else
      echo "Failed to download from $url"
      rm -f "$target"
    fi
  done

  >&2 echo "Failed to download $fname"
  exit 1
}

# PRIMARY and SMALL (bytes)
fetch_network EvalFileDefaultName 50000000
fetch_network EvalFileDefaultNameSmall 1000000
