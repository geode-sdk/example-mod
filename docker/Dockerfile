# syntax=docker/dockerfile:1.6

ARG MOD_ID
ARG GEODE_VERSION=5.9.0
ARG BINDINGS=geode-sdk/bindings

FROM prevter/geode-sdk:android-${GEODE_VERSION} AS build-repositories
ARG BINDINGS
ARG MOD_ID

WORKDIR /workspace

RUN git clone https://github.com/${BINDINGS} --depth=1 bindings

WORKDIR /workspace/bindings/codegen

RUN --mount=type=cache,target=/workspace/cpm-cache,id=${MOD_ID}-codegen \
    --mount=type=cache,target=/workspace/project/build,id=${MOD_ID}-codegen \
    cmake -G Ninja -B build \
    -DCPM_SOURCE_CACHE=/workspace/cpm-cache \
    && cmake --build build --parallel \
    && cp build/Codegen /workspace/


FROM prevter/geode-sdk:android-${GEODE_VERSION} AS build-android32
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache,id=${MOD_ID}-android32 \
    --mount=type=cache,target=/workspace/project/build,id=${MOD_ID}-android32 \
    cmake --preset docker-a32 \
    && cmake --build --preset docker-a32 \
    && mkdir /workspace/bin \
    && cp build/*.geode /workspace/bin/

FROM scratch AS export-android32
COPY --from=build-android32 /workspace/bin/* .

# Android 64
FROM prevter/geode-sdk:android-${GEODE_VERSION} AS build-android64
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache,id=${MOD_ID}-android64 \
    --mount=type=cache,target=/workspace/project/build,id=${MOD_ID}-android64 \
    cmake --preset docker-a64 \
    && cmake --build --preset docker-a64 \
    && mkdir /workspace/bin \
    && cp build/*.geode /workspace/bin/

FROM scratch AS export-android64
COPY --from=build-android64 /workspace/bin/* .

# iOS
FROM prevter/geode-sdk:ios-${GEODE_VERSION} AS build-ios
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache,id=${MOD_ID}-ios \
    --mount=type=cache,target=/workspace/project/build,id=${MOD_ID}-ios \
    cmake --preset docker-ios \
    && cmake --build --preset docker-ios \
    && mkdir /workspace/bin \
    && cp build/*.geode /workspace/bin/

FROM scratch AS export-ios
COPY --from=build-ios /workspace/bin/* .


# MacOS
FROM prevter/geode-sdk:macos-${GEODE_VERSION} AS build-macos
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache,id=${MOD_ID}-macos \
    --mount=type=cache,target=/workspace/project/build,id=${MOD_ID}-macos \
    cmake --preset docker-mac \
    && cmake --build --preset docker-mac \
    && mkdir /workspace/bin \
    && cp build/*.geode /workspace/bin/

FROM scratch AS export-macos
COPY --from=build-macos /workspace/bin/* .

# Windows
FROM prevter/geode-sdk:windows-${GEODE_VERSION} AS build-windows
ARG MOD_ID

COPY --from=build-repositories /workspace/bindings /workspace/bindings
COPY --from=build-repositories /workspace/Codegen /workspace/project/build/bindings/codegen/Codegen

WORKDIR /workspace/project
COPY . .

RUN --mount=type=cache,target=/workspace/cpm-cache,id=${MOD_ID}-windows \
    --mount=type=cache,target=/workspace/project/build,id=${MOD_ID}-windows \
    cmake --preset docker-win \
    && cmake --build --preset docker-win \
    && mkdir /workspace/bin \
    && cp build/*.geode /workspace/bin/

FROM scratch AS export-windows
ARG MOD_ID
COPY --from=build-windows /workspace/bin/* .

# Combine
FROM prevter/geode-sdk:windows-${GEODE_VERSION} AS build-combine
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

WORKDIR /
COPY docker/extract.sh .
RUN chmod +x /docker-extract.sh
ENTRYPOINT ["/docker-extract.sh"]
