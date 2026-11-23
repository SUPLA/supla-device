// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <SuplaDevice.h>
#include <linux_network.h>
#include <linux_process.h>
#include <supla/control/dimmer_leds.h>
#include <supla/control/rgb_leds.h>
#include <supla/control/rgbw_leds.h>
#include <supla/control/virtual_relay.h>
#include <supla/debug/debug_log_tcp_server.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <supla/version.h>
#include <unistd.h>

// Below includes are added just for CI compilation check. Some of them
// are not used in any cpp file, so they would not be compiled otherwise.
// Remove them and keep only required one in real application.
#include <linux_clock.h>
#include <linux_file_state_logger.h>
#include <linux_file_storage.h>
#include <linux_mqtt_client.h>
#include <linux_yaml_config.h>
#include <supla/IEEE754tools.h>
#include <supla/action_handler.h>
#include <supla/actions.h>
#include <supla/at_channel.h>
#include <supla/channel.h>
#include <supla/channel_element.h>
#include <supla/channel_extended.h>
#include <supla/condition.h>
#include <supla/condition_getter.h>
#include <supla/correction.h>
#include <supla/crc16.h>
#include <supla/definitions.h>
#include <supla/element.h>
#include <supla/events.h>
#include <supla/io.h>
#include <supla/local_action.h>
#include <supla/parser/simple.h>
#include <supla/pv/afore.h>
#include <supla/pv/fronius.h>
#include <supla/rsa_verificator.h>
#include <supla/sensor/binary.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/sensor/distance.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/sensor/electricity_meter_parsed.h>
#include <supla/sensor/general_purpose_channel_base.h>
#include <supla/sensor/hygro_meter.h>
#include <supla/sensor/impulse_counter_parsed.h>
#include <supla/sensor/normally_open.h>
#include <supla/sensor/one_phase_electricity_meter.h>
#include <supla/sensor/pressure.h>
#include <supla/sensor/rain.h>
#include <supla/sensor/therm_hygro_meter.h>
#include <supla/sensor/therm_hygro_press_meter.h>
#include <supla/sensor/thermometer.h>
#include <supla/sensor/thermometer_parsed.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/weight.h>
#include <supla/sensor/wind.h>
#include <supla/source/cmd.h>
#include <supla/source/file.h>
#include <supla/supla_lib_config.h>
#include <supla/timer.h>
#include <supla/tools.h>
#include <supla/uptime.h>

#include <cstdlib>
#include <cxxopts.hpp>
#include <iostream>
#include <memory>
#include <string>

#include "debug_socket.h"
#include "sleepy_thermometer.h"
#include "suplan_runtime_linux.h"

// reguired by linux_log.c
int logLevel = LOG_INFO;
int runAsDaemon = 0;

int main(int argc, char *argv[]) {
  try {
    cxxopts::Options options(argv[0], "Supla device client. See www.supla.org");

    options.add_options()("D,debug", "Enable debug logs")(
        "V,verbose",
        "Enable verbose debug logs",
        cxxopts::value<bool>()->default_value("false"))(
        "i,integer", "Int param", cxxopts::value<int>())(
        "c,config",
        "Config file name",
        cxxopts::value<std::string>()->default_value("etc/supla-device.yaml"))(
        "debug-socket",
        "Read insecure debug command JSON lines from Unix socket path",
        cxxopts::value<std::string>()->default_value(""))(
        "debug-log-port",
        "Stream SUPLA logs over insecure TCP port, 0 disables",
        cxxopts::value<uint16_t>()->default_value("0"))(
        "sleepy-cycle", "One cold-boot thermometer cycle (positive ID)",
        cxxopts::value<uint64_t>())(
        "sleepy-ready-ms", "Maximum wait for UDP and Source READ grant",
        cxxopts::value<uint32_t>()->default_value("15000"))(
        "sleepy-awake-ms", "Advertised wake window in milliseconds",
        cxxopts::value<uint16_t>()->default_value("10000"))(
        "sleepy-tail-ms", "Sleep announcement processing time in milliseconds",
        cxxopts::value<uint32_t>()->default_value("1000"))(
        "sleepy-next-ms", "Next planned start (CLOCK_BOOTTIME milliseconds)",
        cxxopts::value<uint64_t>()->default_value("0"))(
        "d,daemon", "Run in daemon mode (run in background and log to syslog)")(
        "s,service", "Run as a service (log to syslog but don't fork)")(
        "h,help", "Show this help")("v,version", "Show version");

    auto result = options.parse(argc, argv);

    if (result.count("help")) {
      std::cout << options.help() << std::endl;
      exit(0);
    }

    if (result.count("version")) {
      std::cout << argv[0] << " version: " << SUPLA_SHORT_VERSION << std::endl;
      exit(0);
    }

    std::unique_ptr<Supla::SleepyThermometer> sleepy;
    if (result.count("sleepy-cycle")) {
      const auto cycle = result["sleepy-cycle"].as<uint64_t>();
      const auto ready = result["sleepy-ready-ms"].as<uint32_t>();
      const auto awake = result["sleepy-awake-ms"].as<uint16_t>();
      const auto tail = result["sleepy-tail-ms"].as<uint32_t>();
      const auto next = result["sleepy-next-ms"].as<uint64_t>();
      if (!cycle || !ready || ready > 3600000 || !awake || !tail ||
          tail > 30000 || !next || result.count("daemon") ||
          result.count("service")) {
        SUPLA_LOG_ERROR("Invalid foreground sleepy cycle options");
        return 1;
      }
      sleepy = std::make_unique<Supla::SleepyThermometer>(
          cycle, ready, awake, tail, next);
    }

    if (result.count("daemon")) {
      runAsDaemon = 1;
      if (!st_try_fork()) {
        SUPLA_LOG_ERROR("Can't start daemon");
        exit(1);
      }
    }

    if (result.count("service") && result.count("daemon")) {
      SUPLA_LOG_ERROR("Can't use daemon and service mode at the same time");
      exit(1);
    }

    if (result.count("service")) {
      runAsDaemon = true;  // just for using syslog
      if ((chdir("/")) < 0) {
        SUPLA_LOG_ERROR("Can't start as a service");
        exit(1);
      }

      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      close(STDERR_FILENO);
    }

    std::string cfgFile = result["config"].as<std::string>();
    auto config = std::make_unique<Supla::LinuxYamlConfig>(cfgFile);
    SUPLA_LOG_INFO("Startup: sd4linux config");
    // we init it earlier than in SuplaDevice.begin, because it is needed
    // earlier
    {
      Supla::LogIndent configIndent;
      SUPLA_LOG_INFO("Config file: %s", cfgFile.c_str());
      if (!config->init()) {
        SUPLA_LOG_ERROR("Configuration initialization failed. Exit");
        exit(1);
      }
    }

    if (result.count("debug") || config->isDebug()) {
      logLevel = LOG_DEBUG;
    }

    if (result.count("verbose") || config->isVerbose()) {
      logLevel = LOG_VERBOSE;
    }

    if (result.count("warning") || config->isWarning()) {
      logLevel = LOG_WARNING;
    }

    if (result.count("error") || config->isError()) {
      logLevel = LOG_ERR;
    }
    SuplaDevice.setLogLevel(logLevel);

    st_hook_signals();

    SUPLA_LOG_INFO("Startup: clock");
    std::unique_ptr<Supla::LinuxClock> clock;
    {
      Supla::LogIndent clockIndent;
      clock = std::make_unique<Supla::LinuxClock>();
    }

    SUPLA_LOG_INFO("Startup: channel setup");
    {
      Supla::LogIndent channelsIndent;
      if (!config->loadChannels()) {
        SUPLA_LOG_ERROR("Loading channels failed. Exit");
        exit(1);
      }
    }

    Supla::LinuxSupLanConfig suplanConfig;
    if (!config->getSupLanConfig(&suplanConfig)) {
      SUPLA_LOG_ERROR("SupLAN YAML configuration is invalid. Exit");
      exit(1);
    }

    if (sleepy && (!suplanConfig.enabled || !suplanConfig.serverProvisioning)) {
      SUPLA_LOG_ERROR("Sleepy thermometer requires SupLAN SERVER mode");
      return 1;
    }

    Supla::LinuxFileStorage storage(config->getStateFilesPath());

    SuplaDevice.setLastStateLogger(
        new Supla::Device::FileStateLogger(config->getStateFilesPath()));
    Supla::LinuxNetwork network;

    std::unique_ptr<Supla::LinuxSupLanRuntime> suplanRuntime;
    if (suplanConfig.enabled) {
      suplanRuntime.reset(new Supla::LinuxSupLanRuntime(suplanConfig));
      if (!suplanRuntime->initialize()) {
        SUPLA_LOG_ERROR("SupLAN runtime initialization failed. Exit");
        exit(1);
      }
      if (suplanConfig.serverProvisioning) {
        SUPLA_LOG_INFO("SupLAN SERVER provisioning enabled port=%u",
                       suplanConfig.unicastPort);
#ifdef SUPLAN_TESTING
      } else {
        SUPLA_LOG_INFO("SupLAN PoC fixture enabled role=%c port=%u",
                       suplanConfig.role, suplanConfig.unicastPort);
#endif  // SUPLAN_TESTING
      }
    }

    SUPLA_LOG_INFO("Startup: suplet setup");
    {
      Supla::LogIndent supletsIndent;
      if (!setupLinuxSupletRuntime(config.get())) {
        SUPLA_LOG_ERROR("Suplet runtime setup failed. Exit");
        exit(1);
      }
    }

    SuplaDevice.setProtoVerboseLog(config->isProtoVerboseLog());
    SuplaDevice.begin(config->getProtoVersion());

    if (SuplaDevice.getCurrentStatus() != STATUS_INITIALIZED) {
      SUPLA_LOG_INFO("Incomplete configuration. Please fix it and try again");
      exit(1);
    }

    if (sleepy && !sleepy->initialize()) {
      return sleepy->finish(SuplaDevice.prepareForShutdown(), false);
    }

    SUPLA_LOG_INFO("Startup: debug interfaces");
    {
      Supla::LogIndent debugIndent;
      if (!initLinuxDebugSocket(result["debug-socket"].as<std::string>(),
                                suplanRuntime.get())) {
        SUPLA_LOG_ERROR("Debug socket init failed. Exit");
        exit(1);
      }
    }
    Supla::Debug::DebugLogTcpServer debugLogServer(
        result["debug-log-port"].as<uint16_t>());
    {
      Supla::LogIndent debugIndent;
      debugLogServer.begin();
    }

    SUPLA_LOG_INFO("Startup: MQTT");
    {
      Supla::LogIndent mqttIndent;
      Supla::LinuxMqttClient::start();
    }
    SUPLA_LOG_INFO("Startup: sd4linux ready");

    while (st_app_terminate == 0) {
      iterateLinuxDebugSocket();
      debugLogServer.iterate();
      SuplaDevice.iterate();
      if (sleepy && !sleepy->iterate(*suplanRuntime)) break;
      delay(10);
    }
    SUPLA_LOG_INFO("Exit");
    bool saved = SuplaDevice.prepareForShutdown();
    if (!saved) {
      SUPLA_LOG_ERROR("Configuration save failed during shutdown");
    }
    if (sleepy) exit(sleepy->finish(saved, st_app_terminate != 0));
    exit(saved ? 0 : 1);
  } catch (const cxxopts::exceptions::exception &e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    exit(1);
  }
}
