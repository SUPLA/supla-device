/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include <supla-common/tools.h>
#include <linux_network.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <cxxopts.hpp>

#include <SuplaDevice.h>
#include <supla/control/dimmer_leds.h>
#include <supla/control/rgb_leds.h>
#include <supla/control/rgbw_leds.h>
#include <supla/control/virtual_relay.h>
#include <supla/time.h>
#include <supla/version.h>
#include <supla/log_wrapper.h>
#include <supla/device/supla_ca_cert.h>

// Below includes are added just for CI compilation check. Some of them
// are not used in any cpp file, so they would not be compiled otherwise.
// Remove them and keep only required one in real application.
#include <linux_file_state_logger.h>
#include <linux_yaml_config.h>
#include <linux_file_storage.h>
#include <linux_clock.h>
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
#include <supla/parser/json.h>
#include <supla/parser/simple.h>
#include <supla/pv/afore.h>
#include <supla/pv/fronius.h>
#include <supla/rsa_verificator.h>
#include <supla/sensor/binary.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/sensor/distance.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/sensor/electricity_meter_parsed.h>
#include <supla/sensor/general_purpose_measurement_base.h>
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
#include <supla/sha256.h>
#include <supla/source/cmd.h>
#include <supla/source/file.h>
#include <supla/supla_lib_config.h>
#include <supla/timer.h>
#include <supla/tools.h>
#include <supla/uptime.h>


// reguired by linux_log.c
int logLevel = LOG_INFO;
int runAsDaemon = 0;

int main(int argc, char* argv[]) {
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
    // we init it earlier than in SuplaDevice.begin, because it is needed
    // earlier
    config->init();

    if (result.count("debug") || config->isDebug()) {
      logLevel = LOG_DEBUG;
    }

    if (result.count("verbose") || config->isVerbose()) {
      logLevel = LOG_VERBOSE;
    }

    SUPLA_LOG_INFO(" *** Starting supla-device ***");
    SUPLA_LOG_INFO("Using config file %s", cfgFile.c_str());

    st_hook_signals();

    if (!config->loadChannels()) {
      SUPLA_LOG_ERROR("Loading channels failed. Exit");
      exit(1);
    }

    Supla::LinuxFileStorage storage(config->getStateFilesPath());

    SuplaDevice.setLastStateLogger(
        new Supla::Device::FileStateLogger(config->getStateFilesPath()));
    Supla::LinuxNetwork network;

    // configure defualt Supla CA certificate
    SuplaDevice.setSuplaCACert(suplaCACert);
    SuplaDevice.setSupla3rdPartyCACert(supla3rdCACert);

    auto clock = std::make_unique<Supla::LinuxClock>();
    (void)(clock);

    SuplaDevice.begin(config->getProtoVersion());

    if (SuplaDevice.getCurrentStatus() != STATUS_INITIALIZED) {
      SUPLA_LOG_INFO("Incomplete configuration. Please fix it and try again");
      exit(1);
    }

    while (st_app_terminate == 0) {
      SuplaDevice.iterate();
      delay(10);
    }
    SUPLA_LOG_INFO("Exit");

    exit(0);
  } catch (const cxxopts::OptionException& e) {
    std::cout << "error parsing options: " << e.what() << std::endl;
    exit(1);
  }
}
