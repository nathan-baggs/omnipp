FROM ubuntu:24.04
WORKDIR /mnt
ENV TARGET=x86_64-linux-musl
ENV SYSROOT=/usr/local/${TARGET}
ARG PKG_CONFIG_VERSION=0.29.2
ARG CMAKE_VERSION=4.1.2
ARG BINUTILS_VERSION=2.45
ARG MUSL_VERSION=1.2.5
ARG GCC_VERSION=trunk
ARG NASM_VERSION=3.01
ARG LLVM_VERSION=21.1.0  

RUN ln -sf /bin/bash /bin/sh
RUN set -ex \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get upgrade --no-install-recommends -y \
    && DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y \
    ca-certificates gcc g++ zlib1g-dev libssl-dev libgmp-dev libmpfr-dev \
    libmpc-dev libisl-dev libssl3 libgmp10 libmpfr6 libmpc3 libisl23 \
    xz-utils ninja-build texinfo meson gnupg bzip2 patch gperf bison \
    file flex make yasm wget zip git jq curl python3 ccache

RUN mkdir -p ${SYSROOT} \
    && chmod 0777 -R /mnt ${SYSROOT}

RUN wget -q https://pkg-config.freedesktop.org/releases/pkg-config-${PKG_CONFIG_VERSION}.tar.gz -O - | tar -xz \
    && cd pkg-config-${PKG_CONFIG_VERSION} \
    && ./configure \
        --prefix=/usr/local \
        --with-internal-glib \
        --with-pc-path=${SYSROOT}/lib/pkgconfig \
        --disable-nls \
    && make -j`nproc` \
    && make install \
    && cd .. \
    && rm -r pkg-config-${PKG_CONFIG_VERSION}

RUN wget -q https://ftp.gnu.org/gnu/binutils/binutils-${BINUTILS_VERSION}.tar.xz -O - | tar -xJ \
    && cd binutils-${BINUTILS_VERSION} \
    && ./configure \
        --prefix=/usr/local \
        --target=${TARGET} \
        --disable-shared \
        --enable-static \
        --disable-plugins \
        --disable-multilib \
        --disable-nls \
        --disable-werror \
        --with-system-zlib \
    && make -j`nproc` \
    && make install \
    && cd .. \
    && rm -r binutils-${BINUTILS_VERSION}

RUN wget -q https://musl.libc.org/releases/musl-${MUSL_VERSION}.tar.gz -O - | tar -xz \
    && cd musl-${MUSL_VERSION} \
    && ./configure \
        --prefix=${SYSROOT} \
        --enable-static \
        --enable-shared \
    && make -j`nproc` \
    && make install \
    && cd .. \
    && rm -r musl-${MUSL_VERSION}

RUN cp -r /usr/include/linux ${SYSROOT}/include/ \
    && cp -r /usr/include/asm-generic ${SYSROOT}/include/ \
    && mkdir -p ${SYSROOT}/include/asm \
    && cp -r /usr/include/x86_64-linux-gnu/asm/* ${SYSROOT}/include/asm/

RUN if [ "${GCC_VERSION}" = "trunk" ]; then \
        git clone --depth 1 git://gcc.gnu.org/git/gcc.git gcc-${GCC_VERSION}; \
    else \
        wget -q https://ftp.gnu.org/gnu/gcc/gcc-${GCC_VERSION}/gcc-${GCC_VERSION}.tar.xz -O - | tar -xJ; \
    fi \
    && mkdir gcc \
    && cd gcc \
    && ln -sf . ${SYSROOT}/usr \
    && ../gcc-${GCC_VERSION}/configure \
        --prefix=/usr/local \
        --target=${TARGET} \
        --enable-languages=c,c++ \
        --enable-shared \
        --enable-static \
        --with-system-zlib \
        --enable-libgomp \
        --enable-libatomic \
        --enable-graphite \
        --enable-libsanitizer \
        --disable-multilib \
        --disable-nls \
        --disable-werror \
    && make -j`nproc` all-gcc \
    && make install-gcc \
    && cd ..
RUN cd gcc \
    && make -j`nproc` CXXFLAGS_FOR_TARGET="-DPATH_MAX=4096" \
    && make install \
    && cd .. \
    && rm -r gcc gcc-${GCC_VERSION}

RUN wget -q https://github.com/rui314/mold/releases/download/v2.41.0/mold-2.41.0-x86_64-linux.tar.gz \
    && tar -xzf mold-2.41.0-x86_64-linux.tar.gz \
    && cp mold-2.41.0-x86_64-linux/bin/mold /usr/local/bin/ \
    && cp -r mold-2.41.0-x86_64-linux/libexec/mold /usr/local/libexec/ \
    && ln -sf /usr/local/bin/mold /usr/local/bin/ld.mold \
    && rm -r mold-2.41.0-x86_64-linux*

RUN wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}.tar.gz -O - | tar -xz \
    && cd cmake-${CMAKE_VERSION} \
    && ./configure --prefix=/usr/local --parallel=`nproc` \
    && make -j`nproc` \
    && make install \
    && cd .. \
    && rm -r cmake-${CMAKE_VERSION}

RUN wget -q https://github.com/llvm/llvm-project/releases/download/llvmorg-${LLVM_VERSION}/llvm-project-${LLVM_VERSION}.src.tar.xz -O - | tar -xJ \
    && cd llvm-project-${LLVM_VERSION}.src \
    && mkdir build \
    && cd build \
    && cmake -G Ninja ../llvm \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DLLVM_ENABLE_PROJECTS="lld" \
    && ninja lld \
    && ninja install \
    && cd ../.. \
    && rm -r llvm-project-${LLVM_VERSION}.src

RUN wget -q https://www.nasm.us/pub/nasm/releasebuilds/${NASM_VERSION}/nasm-${NASM_VERSION}.tar.xz -O - | tar -xJ \
    && cd nasm-${NASM_VERSION} \
    && ./configure --prefix=/usr/local \
    && make -j`nproc` \
    && make install \
    && cd .. \
    && rm -r nasm-${NASM_VERSION}

RUN apt-get remove --purge -y file gcc g++ zlib1g-dev libssl-dev libgmp-dev libmpfr-dev libmpc-dev libisl-dev

ARG TREE_SITTER_CLI_VERSION=0.26.10

RUN set -ex \
    && apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install --no-install-recommends -y unzip \
    && ARCH=$(uname -m | sed 's/x86_64/x64/;s/aarch64/arm64/') \
    && if [ "$ARCH" != "x64" ] && [ "$ARCH" != "arm64" ]; then echo "Unsupported architecture: $(uname -m)"; exit 1; fi \
    && wget -q "https://github.com/tree-sitter/tree-sitter/releases/download/v${TREE_SITTER_CLI_VERSION}/tree-sitter-cli-linux-${ARCH}.zip" \
    && unzip "tree-sitter-cli-linux-${ARCH}.zip" \
    && mv tree-sitter /usr/local/bin/tree-sitter \
    && chmod +x /usr/local/bin/tree-sitter \
    && rm "tree-sitter-cli-linux-${ARCH}.zip" \
    && apt-get remove --purge -y unzip \
    && tree-sitter --version

RUN mkdir -p /ccache && chmod 1777 /ccache

RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    libtool \
    ragel \
    kelbt
