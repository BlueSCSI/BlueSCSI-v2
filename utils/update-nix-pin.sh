#!/usr/bin/env bash

# BlueSCSI (C) 2025 Eric Helgeson

echo "Getting current nixpkgs version..."
version=$(nix-instantiate --eval -E '(import <nixpkgs> {}).lib.version' | sed 's/"//g')
echo "Version: $version"
rev=$(echo "$version" | awk -F. '{print $NF}')
echo "Revision: $rev"
echo "Prefetching tarball to get sha256 hash..."
hash=$(nix-prefetch-url --unpack "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz")
echo ""
echo "Update your shell.nix with these values:"
echo "nixpkgs_rev = \"${rev}\";"
echo "nixpkgs_hash = \"${hash}\";"
