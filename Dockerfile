# syntax=docker/dockerfile:1.6

ARG MOD_ID
ARG SDK_VERSION=5.9.0
ARG BINDINGS=geode-sdk/bindings
ARG GIT_INVALIDATE_CLONE=0

FROM prevter/geode-sdk:android-${SDK_VERSION} AS build-repositories
ARG BINDINGS
ARG GIT_INVALIDATE_CLONE

WORKDIR /workspace

RUN echo "Cloning Bindings (invalidate=${GIT_INVALIDATE_CLONE})" && \
    git clone https://github.com/${BINDINGS} --depth=1 bindings

WORKDIR /workspace/bindings/codegen

RUN --mount=type=cache,target=/workspace/cpm-cache \
    --mount=type=cache,target=/workspace/project/build,id=android32 \
    cmake -G Ninja -B build \
    -DCPM_SOURCE_CACHE=/workspace/cpm-cache \
    && cmake --build build --parallel \
    && cp build/Codegen /workspace/


FROM prevter/geode-sdk:android-${SDK_VERSION} AS build-android32
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    --mount=type=cache,target=/workspace/project/build,id=android32 \
    cmake --preset docker-a32 \
    && cmake --build --preset docker-a32

FROM scratch AS export-android32
ARG MOD_ID
COPY --from=build-android32 /workspace/project/build/${MOD_ID}.geode .
COPY --from=build-android32 /workspace/project/build/${MOD_ID}.android32.debug.so .

# Android 64
FROM prevter/geode-sdk:android-${SDK_VERSION} AS build-android64
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    --mount=type=cache,target=/workspace/project/build,id=android64 \
    cmake --preset docker-a64 \
    && cmake --build --preset docker-a64

FROM scratch AS export-android64
ARG MOD_ID
COPY --from=build-android64 /workspace/project/build/${MOD_ID}.geode .
COPY --from=build-android64 /workspace/project/build/${MOD_ID}.android64.debug.so .

# iOS
FROM prevter/geode-sdk:ios-${SDK_VERSION} AS build-ios
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    --mount=type=cache,target=/workspace/project/build,id=ios \
    cmake --preset docker-ios \
    && cmake --build --preset docker-ios

FROM scratch AS export-ios
ARG MOD_ID
COPY --from=build-ios /workspace/project/build/${MOD_ID}.geode .
COPY --from=build-ios /workspace/project/build/${MOD_ID}.ios.debug.dylib .


# MacOS
FROM prevter/geode-sdk:macos-${SDK_VERSION} AS build-macos
ARG MOD_ID
ARG CPM_CACHE_DIR

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    --mount=type=cache,target=/workspace/project/build,id=macos \
    cmake --preset docker-mac \
    && cmake --build --preset docker-mac

FROM scratch AS export-macos
ARG MOD_ID
COPY --from=build-macos /workspace/project/build/${MOD_ID}.geode .
COPY --from=build-macos /workspace/project/build/${MOD_ID}.debug.dylib .

# Windows
FROM prevter/geode-sdk:windows-${SDK_VERSION} AS build-windows
ARG MOD_ID
ARG CPM_CACHE_DIR

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache \
    --mount=type=cache,target=/workspace/project/build,id=windows \
    cmake --preset docker-win \
    && cmake --build --preset docker-win

FROM scratch AS export-windows
ARG MOD_ID
COPY --from=build-windows /workspace/project/build/${MOD_ID}.geode .
COPY --from=build-windows /workspace/project/build/${MOD_ID}.pdb .

# Combine
FROM prevter/geode-sdk:windows-${SDK_VERSION} AS build-combine
ARG MOD_ID

WORKDIR /workspace

COPY --from=export-windows /${MOD_ID}.geode ${MOD_ID}.geode
COPY --from=export-android32 /${MOD_ID}.geode ${MOD_ID}-a32.geode
COPY --from=export-android64 /${MOD_ID}.geode ${MOD_ID}-a64.geode
COPY --from=export-macos /${MOD_ID}.geode ${MOD_ID}-mac.geode
COPY --from=export-ios /${MOD_ID}.geode ${MOD_ID}-ios.geode

RUN geode package merge ${MOD_ID}.geode ${MOD_ID}-a32.geode ${MOD_ID}-a64.geode ${MOD_ID}-mac.geode ${MOD_ID}-ios.geode

FROM debian:bookworm-slim AS export-combine
ARG MOD_ID

WORKDIR /workspace/export
COPY --from=build-combine /workspace/${MOD_ID}.geode .
COPY --from=export-android32 /${MOD_ID}.android32.debug.so .
COPY --from=export-android64 /${MOD_ID}.android64.debug.so .
COPY --from=export-ios /${MOD_ID}.ios.debug.dylib .
COPY --from=export-macos /${MOD_ID}.debug.dylib .
COPY --from=export-windows /${MOD_ID}.pdb .

WORKDIR /
COPY docker-extract.sh .
RUN chmod +x /docker-extract.sh
ENTRYPOINT ["/docker-extract.sh"]
