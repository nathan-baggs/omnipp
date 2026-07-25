# syntax=docker/dockerfile:1
#
# Self-contained, multi-stage build for a Ragel 6.x release. Regenerates the
# parser from source (bootstrap ragel + kelbt), runs the C/C++/Go test suite,
# builds the guide, and writes to the --output dir:
#   ragel-<ver>.tar.gz     source dist; bundles the PDF and pre-generated parser
#                          sources (end users need neither kelbt nor pdflatex)
#   ragel-guide-<ver>.pdf  the guide
#   SHA256SUMS
#
# Build from the repo root (BuildKit required; default with the buildx plugin):
#
#   docker build -f package/Dockerfile \
#       --build-arg RAGEL_VERSION=6.11 --build-arg PUBDATE="June 2026" \
#       --output type=local,dest=./dist .
#
# Omit the build-args to build whatever configure.ac declares. Build args:
#   RAGEL_VERSION   if set, rewrites AC_INIT(ragel, ...)  (tarball name + banner)
#   PUBDATE         if set, rewrites PUBDATE=...           (guide title + banner)
#   KELBT_VERSION   kelbt release to fetch                 (default 0.16)
#   UBUNTU_TAG      base image                             (default 18.04; 22.04 works)
#   BUILD_PLATFORM  build architecture                     (default linux/amd64)
#
# The version/date args patch configure.ac inside the image only. For a real
# release, commit the same bump, tag ragel-<ver>, and build without the args so
# the tarball is reproducible from VCS.
#
# After the build, sign and publish on the host:
#   gpg --armor --detach-sign dist/ragel-<ver>.tar.gz
# then upload the .tar.gz, .tar.gz.asc and ragel-guide-<ver>.pdf to
# colm.net/files/ragel/ and update the download page.

ARG UBUNTU_TAG=18.04
# amd64 by default: ragel/kelbt are x86-era C++ that only compile where char is
# signed, so they fail on arm64. To build native arm64, set BUILD_PLATFORM to
# linux/arm64 and force signed char (add ENV CFLAGS=-fsigned-char
# CXXFLAGS=-fsigned-char to the kelbt and builder stages).
ARG BUILD_PLATFORM=linux/amd64

# Base: C/C++ toolchain, autotools, bootstrap ragel. 18.04 suits the 6.10 era;
# 22.04 also works (--build-arg UBUNTU_TAG=22.04).
FROM --platform=${BUILD_PLATFORM} ubuntu:${UBUNTU_TAG} AS base
ENV DEBIAN_FRONTEND=noninteractive
# Fall back to old-releases for EOL bases (handles amd64 and arm64 mirror paths).
RUN set -eux; \
    apt-get update 2>/dev/null || { \
        sed -i -e 's|//archive.ubuntu.com/ubuntu|//old-releases.ubuntu.com/ubuntu|g' \
               -e 's|//security.ubuntu.com/ubuntu|//old-releases.ubuntu.com/ubuntu|g' \
               -e 's|//ports.ubuntu.com/ubuntu-ports|//old-releases.ubuntu.com/ubuntu-ports|g' \
               /etc/apt/sources.list; \
        apt-get update; \
    }; \
    apt-get install -y --no-install-recommends \
        build-essential autoconf automake ragel git wget ca-certificates; \
    rm -rf /var/lib/apt/lists/*

# kelbt: the only build dependency not in apt.
FROM base AS kelbt
ARG KELBT_VERSION=0.16
WORKDIR /tmp/kelbt
RUN wget -q "https://www.colm.net/files/kelbt/kelbt-${KELBT_VERSION}.tar.gz" \
    && tar --strip-components=1 -xf "kelbt-${KELBT_VERSION}.tar.gz" \
    && ./configure \
    && make \
    && make install \
    && ldconfig

# Regenerate parsers, build the manual, roll the distribution.
FROM base AS builder
# Manual toolchain: pdflatex (+ latex-extra for comment.sty), the fig->pdf
# converter (fig2dev on 20.04+, transfig on 18.04), and ghostscript for it.
RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends \
        texlive-latex-base texlive-latex-recommended texlive-latex-extra \
        texlive-fonts-recommended ghostscript; \
    apt-get install -y --no-install-recommends fig2dev \
        || apt-get install -y --no-install-recommends transfig; \
    rm -rf /var/lib/apt/lists/*

# Test toolchain for the gating suite: Go (C/C++ are already in base; the suite
# auto-skips any language whose compiler is absent).
RUN set -eux; \
    apt-get update; \
    apt-get install -y --no-install-recommends golang-go; \
    rm -rf /var/lib/apt/lists/*

COPY --from=kelbt /usr/local/ /usr/local/
RUN ldconfig

WORKDIR /src
# Copy the tree and reduce it to a pristine checkout: drops build cruft, keeps
# uncommitted edits to tracked files (e.g. a configure.ac version bump).
COPY . .
RUN git config --global --add safe.directory /src && git clean -xdf

# Optional version stamp; omit the args to build what configure.ac declares.
ARG RAGEL_VERSION=
ARG PUBDATE=
RUN if [ -n "$RAGEL_VERSION" ]; then \
        sed -i "s/^AC_INIT(ragel, .*/AC_INIT(ragel, ${RAGEL_VERSION})/" configure.ac; \
    fi; \
    if [ -n "$PUBDATE" ]; then \
        sed -i "s/^PUBDATE=.*/PUBDATE=\"${PUBDATE}\"/" configure.ac; \
    fi

# Regenerate configure and build the parser (kelbt+ragel), the ragel binary,
# and the guide PDF.
RUN chmod +x autogen.sh \
    && ./autogen.sh \
    && ./configure \
    && make

# Gate: run the C/C++/Go test suite before producing any artifacts; any failure
# fails the build. -C covers C/C++ (and obj-C, which skips with no compiler
# installed), -Z is Go; the full -T0..-G2 x -n/-m/-l/-e matrix is exercised per
# case. D is excluded: gdc 8.x rejects the 2009-era D test sources and codegen
# (pre-existing language drift, unrelated to the 6.11 changes).
RUN cd test && ./runtests -C -Z

# Roll the tarball and collect the publishable artifacts.
RUN set -eu; \
    make dist; \
    mkdir -p /artifacts; \
    cp ragel-*.tar.gz /artifacts/; \
    ver="$(ls ragel-*.tar.gz | sed -n 's/^ragel-\(.*\)\.tar\.gz$/\1/p')"; \
    cp doc/ragel-guide.pdf "/artifacts/ragel-guide-${ver}.pdf"; \
    cd /artifacts && sha256sum ragel-*.tar.gz ragel-guide-*.pdf > SHA256SUMS

# Artifacts-only stage, so --output writes just these.
FROM scratch AS export
COPY --from=builder /artifacts/ /
