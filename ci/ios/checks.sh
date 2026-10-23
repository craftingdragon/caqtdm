#!/usr/bin/env bash

ci_error() {
  echo "::error::$*" >&2
  return 1
}

require_file() {
  [[ -f "$1" ]] || ci_error "Missing file: $1"
}

require_nonempty_file() {
  [[ -s "$1" ]] || ci_error "Missing or empty file: $1"
}

require_dir() {
  [[ -d "$1" ]] || ci_error "Missing directory: $1"
}

require_executable() {
  [[ -x "$1" ]] || ci_error "Missing or nonexecutable file: $1"
}

require_equal() {
  [[ "$1" == "$2" ]] || ci_error "$3: expected '$2', got '$1'"
}

require_nonempty() {
  [[ -n "$1" ]] || ci_error "$2 is empty"
}

require_arch() {
  local file="$1" arch="$2" arches
  arches="$(lipo -archs "$file")" || ci_error "Cannot read architectures in $file"
  [[ " ${arches} " == *" ${arch} "* ]] || ci_error "$file: expected architecture $arch, found ${arches}"
}

