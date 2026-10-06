# Build environment for PS5 payloads (ps5-payload-sdk + pacbrew libraries).
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=C.UTF-8 \
    LC_ALL=C.UTF-8
RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake pkg-config meson ninja-build clang-18 lld-18 llvm-18 clang lld llvm build-essential \
        autoconf automake libtool yasm nasm bison flex gperf pkgconf \
        libarchive-tools autopoint po4a git curl wget ca-certificates doxygen \
        makepkg pacman-package-manager python3 python3-mako python3-glad \
        python3-pyelftools socat sudo unzip ccache file \
    && rm -rf /var/lib/apt/lists/*

# makepkg refuses to run as root, so build as an unprivileged user with sudo.
RUN useradd -m -s /bin/bash builder \
    && echo 'builder ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/builder \
    && mkdir -p /var/lib/pacman /var/cache/pacman/pkg

ENV PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
USER builder
WORKDIR /home/builder

COPY --chown=builder:builder docker/build-deps.sh /home/builder/build-deps.sh
ARG PACBREW_REV=main
RUN git clone https://github.com/ps5-payload-dev/pacbrew-repo \
    && git -C pacbrew-repo checkout ${PACBREW_REV} \
    && MAKEFLAGS=-j$(nproc) ./build-deps.sh pacbrew-repo \
    && rm -rf pacbrew-repo/*/src pacbrew-repo/*/pkg

# Packages not in pacbrew-repo, in the same PKGBUILD format.
COPY --chown=builder:builder deps/ /home/builder/deps/
RUN for PKG in icu nlohmann-json SDL2; do \
        cd /home/builder/deps/${PKG} \
        && MAKEFLAGS=-j$(nproc) makepkg -c -f -C --nocheck \
        && sudo pacman --config /home/builder/pacbrew-repo/pacman.conf --noconfirm -U ./ps5-payload-*.pkg.tar.* \
        && rm -rf src pkg ./*.pkg.tar.* ./*.tgz ./*.zip \
        || exit 1; \
    done

WORKDIR /work
