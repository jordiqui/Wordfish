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
  >&2 printf "%s\n" "Neither wget nor curl is installed." \
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

get_nnue_filename() {
  # Extract nn-<12hex>.nnue from the macro line in evaluate.h
  grep "$1" "$evaluate_file" | grep "#define" | sed -n 's/.*\(nn-[a-z0-9]\{12\}\.nnue\).*/\1/p'
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
      ln -sf "$target" "$link"
      echo "Existing $fname validated, skipping download"
      return 0
    else
      echo "Removing invalid NNUE file: $fname"
      rm -f "$target"
    fi
  fi

  for url in \
    "https://tests.stockfishchess.org/api/nn/$fname" \
    "https://github.com/official-stockfish/networks/raw/master/$fname"; do

    echo "Downloading from $url ..."
    if $download_cmd "$url" > "$target"; then
      if validate_network "$target" "$minsz"; then
        ln -sf "$target" "$link"
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

# BIG and SMALL (bytes)
fetch_network EvalFileDefaultNameBig   50000000
fetch_network EvalFileDefaultNameSmall 1000000
