# Build environment for the payload with ps5-payload-sdk.
#   docker build -t ps5-payload-build .
#   docker run --rm -v "$PWD:/src" ps5-payload-build
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update -qq \
 && apt-get install -y -qq --no-install-recommends clang-18 lld-18 llvm-18 wget unzip make ca-certificates \
 && rm -rf /var/lib/apt/lists/*

# ps5-payload-sdk v0.45 (https://github.com/ps5-payload-dev/sdk), verified by SHA-256
RUN wget -q -O /tmp/sdk.zip https://github.com/ps5-payload-dev/sdk/releases/download/v0.45/ps5-payload-sdk.zip \
 && echo "5fac64093d0e3e69f2584ce7935f24f703f622d9681f1495c85b9f00d1291a2c  /tmp/sdk.zip" | sha256sum -c \
 && unzip -q -d /opt /tmp/sdk.zip \
 && rm /tmp/sdk.zip

ENV PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
ENV LLVM_CONFIG=/usr/bin/llvm-config-18
WORKDIR /src
CMD ["make"]
