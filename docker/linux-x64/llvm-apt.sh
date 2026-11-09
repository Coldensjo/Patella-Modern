#!/bin/sh
# Installs Clang, compiler-rt, clang-format and clang-tidy from LLVM's own apt repository (issue #143;
# notes/decisions.md, "#143"):
#
#	llvm-apt.sh <fetch.sh> <key sha256> <key fingerprint> <major> <version pattern>
#
# Debian trixie has no LLVM 23. apt.llvm.org has, as a rolling snapshot of LLVM's release branch: one version per
# branch in its index and pool, replaced at each publication, so an exact version pin stops working at the next one.
# What is fixed here: the signing key (its file's SHA-256, its fingerprint; apt checks InRelease against it, and the
# index's hashes check each package), the suite llvm-toolchain-trixie-<major>, and the version pattern, which apt
# matches (1:23.1.*: LLVM's 23.1 branch). What is not: which snapshot of the branch; the script prints it.
#
# Retried, as in fetch.sh (issue #153), with a pause (PATELLA_DOWNLOAD_PAUSE seconds times the attempt, up to
# PATELLA_DOWNLOAD_TRIES attempts): apt's whole update and install, since a publication replaces the index and the
# packages, and a package the index has just named can be gone. Stopped at once: a key that does not match, and an
# index without a version of the pattern (a wrong pin).
# The repository's source and key are removed afterwards, so the image's other apt runs only see Debian.

set -eu
if [ $# -ne 5 ]; then
	echo "usage: llvm-apt.sh <fetch.sh> <key sha256> <key fingerprint> <major> <version pattern>" >&2
	exit 2
fi
fetch=$1
key_sha256=$2
key_fingerprint=$3
major=$4
pattern=$5
tries=${PATELLA_DOWNLOAD_TRIES:-10}
pause=${PATELLA_DOWNLOAD_PAUSE:-10}
keyring=/etc/apt/keyrings/llvm-snapshot.gpg
list=/etc/apt/sources.list.d/llvm-toolchain.list
packages="clang-$major libclang-rt-$major-dev clang-format-$major clang-tidy-$major"

sh "$fetch" "$key_sha256" /tmp/llvm-snapshot.gpg.key https://apt.llvm.org/llvm-snapshot.gpg.key
actual=$(gpg --show-keys --with-colons --fingerprint /tmp/llvm-snapshot.gpg.key | awk -F: '$1 == "pub" { want = 1; next } $1 == "fpr" && want { print $10; want = 0 }')
if [ "$actual" != "$key_fingerprint" ]; then
	echo "llvm-apt.sh: the key's fingerprint is '$actual', not $key_fingerprint; not retried" >&2
	exit 1
fi
install -d /etc/apt/keyrings
gpg --dearmor < /tmp/llvm-snapshot.gpg.key > "$keyring"
rm /tmp/llvm-snapshot.gpg.key
echo "deb [signed-by=$keyring] https://apt.llvm.org/trixie/ llvm-toolchain-trixie-$major main" > "$list"

pinned=
for package in $packages; do
	pinned="$pinned $package=$pattern"
done

attempt=1
while :; do
	failed=
	if apt-get -o Acquire::Retries=5 update; then
		# Whether the index can satisfy the pins at all, without a download: a wrong pin is not worth a retry.
		# shellcheck disable=SC2086 # one word per package
		apt-get install -s --no-install-recommends $pinned > /dev/null || {
			echo "llvm-apt.sh: the index cannot satisfy$pinned; not retried" >&2
			apt-cache policy "clang-$major" >&2
			exit 1
		}
		# shellcheck disable=SC2086 # one word per package
		apt-get -o Acquire::Retries=5 install -y --no-install-recommends $pinned || failed=install
	else
		failed=update
	fi
	if [ -z "$failed" ]; then
		break
	fi
	if [ "$attempt" -ge "$tries" ]; then
		echo "llvm-apt.sh: apt-get $failed failed, attempt $attempt of $tries; giving up" >&2
		exit 1
	fi
	step=$attempt
	if [ "$step" -gt 6 ]; then
		step=6
	fi
	wait=$((pause * step))
	echo "llvm-apt.sh: apt-get $failed failed, attempt $attempt of $tries; again in $wait s" >&2
	sleep "$wait"
	attempt=$((attempt + 1))
done

rm -rf /var/lib/apt/lists/*
rm "$list" "$keyring"

# One snapshot: apt's dependencies between these packages say the same version, and this says which.
versions=$(dpkg-query -W -f '${Version}\n' $packages | sort -u)
if [ "$(echo "$versions" | wc -l)" -ne 1 ]; then
	echo "llvm-apt.sh: the packages have different versions:" >&2
	echo "$versions" >&2
	exit 1
fi
echo "llvm-apt.sh: installed $packages at $versions"
