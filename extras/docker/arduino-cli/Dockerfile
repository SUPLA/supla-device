FROM ubuntu:latest
LABEL maintainer="Krzysztof Lewandowski <krzysiek@supla.org>"

# Install required packages
RUN apt update && \
    DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC apt install -y tzdata && \
    apt install -y git wget cmake gcc make libpcap-dev curl \
                   python3 python-is-python3 python3-serial

# Setup workdir
RUN mkdir supla
WORKDIR /supla

# Download and install FreeRTOS
RUN curl -fsSL https://raw.githubusercontent.com/arduino/arduino-cli/master/install.sh | sh
ENV PATH="${PATH}:/supla/bin"
ENV ARDUINO_BOARD_MANAGER_ADDITIONAL_URLS="http://arduino.esp8266.com/stable/package_esp8266com_index.json https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"
RUN arduino-cli core install arduino:avr && \
    arduino-cli lib install Ethernet && \
    arduino-cli core update-index && \
    arduino-cli core install esp32:esp32 && \
    arduino-cli core install esp8266:esp8266 && \
    arduino-cli lib install DallasTemperature && \
    arduino-cli lib install "DHT Sensor Library"  && \
    arduino-cli lib install "Adafruit AHTX0"  && \
    arduino-cli lib install HX711_ADC  && \
    arduino-cli lib install PZEM004Tv30 && \
    cd ~/Arduino/libraries && \
    git clone https://github.com/olehs/PZEM004T

# Download supla-device
RUN git clone https://github.com/SUPLA/supla-device.git && \
    ln -s /supla/supla-device ~/Arduino/libraries/supla-device
WORKDIR /supla/supla-device/examples

ENV SUPLA_DEVICE_PATH="/supla/supla-device/"
ENV SUPLA_TOOLSET="arduino-cli"

