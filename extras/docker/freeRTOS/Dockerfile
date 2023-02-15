FROM ubuntu:latest
LABEL maintainer="Krzysztof Lewandowski <krzysiek@supla.org>"

# Install required packages
RUN apt update && \
    DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt install -y tzdata && \
    apt install -y git wget flex bison gperf python3 python3-pip \
        python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev \
        dfu-util libusb-1.0-0 python-is-python3 gcc make libncurses-dev \
        libpcap-dev libyaml-cpp-dev valgrind

# Setup workdir
RUN mkdir supla
WORKDIR /supla

# Download and install FreeRTOS
RUN git clone https://github.com/FreeRTOS/FreeRTOS.git --recurse-submodules

# Download supla-device
RUN git clone https://github.com/SUPLA/supla-device.git && mkdir projects
WORKDIR /supla/projects

ENV SUPLA_DEVICE_PATH="/supla/supla-device/"
ENV SUPLA_FREERTOS_PATH="/supla/FreeRTOS/"
ENV SUPLA_TOOLSET="freertos"

