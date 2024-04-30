FROM ubuntu:22.04
LABEL authors="Goral64"

ENV SUPLA_DEVICE_PATH=/supla-device

RUN apt-get update -y \
    && apt-get install -y \
    git \
    libssl-dev \
    build-essential \
    libyaml-cpp-dev \
    cmake \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

WORKDIR $SUPLA_DEVICE_PATH

RUN git clone https://github.com/SUPLA/supla-device.git $SUPLA_DEVICE_PATH \
    && cd $SUPLA_DEVICE_PATH/extras/examples/linux \
    && mkdir build  \
    && cd build \
    && cmake .. \
    && make -j5 \
    && mv supla-device-linux / \
    && rm -rf $SUPLA_DEVICE_PATH/*

ENTRYPOINT ["/supla-device-linux", "-c", "conf/supla-device.yaml"]
