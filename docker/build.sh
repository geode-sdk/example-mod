#!/usr/bin/env bash

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/.."

MOD_JSON="${1:-mod.json}"

if [[ ! -f "$MOD_JSON" ]]; then
    echo "Mod json not found at $MOD_JSON" >&2
    exit 1
fi

GEODE_VERSION=$(jq -r '.geode' "$MOD_JSON")

if [[ -z "$GEODE_VERSION" || "$GEODE_VERSION" == "null" ]]; then
    echo "Could not read geode version from $MOD_JSON" >&2
    exit 1
fi

MOD_ID=$(jq -r '.id' "$MOD_JSON")

if [[ -z "$MOD_ID" || "$MOD_ID" == "null" ]]; then
    echo "Could not read mod id from $MOD_JSON" >&2
    exit 1
fi

# export MOD_ID GEODE_VERSION

docker buildx bake $1 --file docker/docker-bake.hcl --set "*.args.MOD_ID=$MOD_ID" --set "*.args.GEODE_VERSION=$GEODE_VERSION"

# docker compose --file compose.yml build build-windows
