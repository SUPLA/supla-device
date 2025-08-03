FROM ubuntu:24.10
LABEL maintainer="Krzysztof Lewandowski <krzysiek@supla.org>"

# Install required packages
RUN apt update && \
    DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt install -y tzdata && \
    apt install -y git wget flex bison gperf python3 python3-pip \
        python3-setuptools cmake ninja-build ccache libffi-dev libssl-dev \
        dfu-util libusb-1.0-0 python-is-python3 gcc make libncurses-dev

# Setup workdir
RUN mkdir /supla
WORKDIR /supla

# Download and install espressif toolset and ESP8266 RTOS SDK
RUN mkdir esp && \
    cd esp && \
    wget https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz && \
    tar -xzf xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz && \
    rm xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz && \
    git clone --recursive https://github.com/espressif/ESP8266_RTOS_SDK.git && \
    /usr/bin/python -m pip install --user -r /supla/esp/ESP8266_RTOS_SDK/requirements.txt

# Download supla-device
RUN git clone https://github.com/SUPLA/supla-device.git && mkdir projects
WORKDIR /supla/projects

ENV SUPLA_DEVICE_PATH="/supla/supla-device"
ENV PATH="${PATH}:/supla/esp/xtensa-lx106-elf/bin"
ENV ESP_TOOLSET="esp8266"
RUN echo "source /supla/esp/ESP8266_RTOS_SDK/export.sh" >> ~/.bashrc

