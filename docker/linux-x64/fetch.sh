#!/bin/sh
# Downloads one pinned file of the image (issue #153; notes/decisions.md, "#153"):
#
#	fetch.sh <sha256> <output> <url> [<fallback url> ...]
#
# GitHub's release downloads answer 504 for hours at times. Retried, with a pause (PATELLA_DOWNLOAD_PAUSE seconds
# times the attempt, up to six times that): HTTP 408, 429 and 5xx, timeouts, a refused or dropped connection, an
# unfinished transfer. A URL with a fallback after it gets PATELLA_DOWNLOAD_FIRST_TRIES attempts (4), the last URL
# PATELLA_DOWNLOAD_TRIES (10). Stopped at once, without trying a fallback: any other HTTP error (a 404 is a wrong pin)
# and a SHA-256 mismatch. <output> only exists after its SHA-256 matched, whichever URL it came from.
# curl's own --retry is not used: it has no fallback, and --retry-all-errors would retry a 404.
# The environment variables are for tools/download_selftest.py.

set -u
if [ $# -lt 3 ]; then
	echo "usage: fetch.sh <sha256> <output> <url> [<fallback url> ...]" >&2
	exit 2
fi
sha256=$1
output=$2
shift 2
part="$output.part"
tries=${PATELLA_DOWNLOAD_TRIES:-10}
first_tries=${PATELLA_DOWNLOAD_FIRST_TRIES:-4}
pause=${PATELLA_DOWNLOAD_PAUSE:-10}

while [ $# -gt 0 ]; do
	url=$1
	shift
	limit=$tries
	if [ $# -gt 0 ]; then
		limit=$first_tries
	fi
	attempt=1
	while :; do
		rm -f "$part"
		http=$(curl -fsSL --connect-timeout 30 --max-time 900 --speed-limit 1024 --speed-time 60 \
			-o "$part" -w '%{http_code}' "$url")
		code=$?
		if [ $code -eq 0 ]; then
			actual=$(sha256sum "$part" | cut -d' ' -f1)
			if [ "$actual" != "$sha256" ]; then
				rm -f "$part"
				echo "fetch.sh: $url has SHA-256 $actual, not $sha256; not retried" >&2
				exit 1
			fi
			mv "$part" "$output"
			exit 0
		fi
		rm -f "$part"
		retry=0
		case $code in
		22)
			case $http in
			408 | 429 | 5??) retry=1 ;;
			esac
			;;
		5 | 6 | 7 | 16 | 18 | 28 | 35 | 52 | 55 | 56 | 92) retry=1 ;;
		esac
		if [ $retry -eq 0 ]; then
			echo "fetch.sh: $url: HTTP $http (curl $code); not retried" >&2
			exit 1
		fi
		if [ "$attempt" -ge "$limit" ]; then
			echo "fetch.sh: $url: HTTP $http (curl $code), attempt $attempt of $limit; giving up on this URL" >&2
			break
		fi
		step=$attempt
		if [ "$step" -gt 6 ]; then
			step=6
		fi
		wait=$((pause * step))
		echo "fetch.sh: $url: HTTP $http (curl $code), attempt $attempt of $limit; again in $wait s" >&2
		sleep "$wait"
		attempt=$((attempt + 1))
	done
done
echo "fetch.sh: every URL failed" >&2
exit 1
