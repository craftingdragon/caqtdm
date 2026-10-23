#!/usr/bin/env bash
set -euo pipefail

if ! latest_tag="$(gh api --paginate --slurp "repos/openssl/openssl/git/matching-refs/tags/openssl-${OPENSSL_LTS_SERIES}." | \
  jq -er --arg series "${OPENSSL_LTS_SERIES}" '
    [.[][] | {tag: (.ref | split("/")[-1]), object: .object}
      | . + {parts: (.tag | sub("^openssl-"; "") | split("."))}
      | select((.parts | length) == 3 and
               ((.parts[0] + "." + .parts[1]) == $series) and
               (.parts[2] | test("^[0-9]+$")))]
    | if length == 0 then empty else sort_by(.parts | map(tonumber)) | last end
    | [.tag, .object.type, .object.sha] | @tsv')"; then
  echo "No numeric OpenSSL ${OPENSSL_LTS_SERIES}.x release tag was found" >&2
  exit 1
fi

IFS=$'\t' read -r openssl_tag object_type object_sha <<< "${latest_tag}"
while [ "${object_type}" = tag ]; do
  tag_object="$(gh api "repos/openssl/openssl/git/tags/${object_sha}")"
  object_type="$(jq -er '.object.type' <<< "${tag_object}")"
  object_sha="$(jq -er '.object.sha' <<< "${tag_object}")"
done
if [ "${object_type}" != commit ]; then
  echo "OpenSSL tag ${openssl_tag} does not resolve to a commit" >&2
  exit 1
fi

openssl_version="${openssl_tag#openssl-}"
echo "version=${openssl_version}" >> "${GITHUB_OUTPUT}"
echo "commit_sha=${object_sha}" >> "${GITHUB_OUTPUT}"
echo "Using OpenSSL LTS tag ${openssl_tag} at commit ${object_sha}"
