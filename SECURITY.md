# Security Policy

## Supported Versions

`supla-device` is developed using a single main development branch. We do not maintain historical source branches as separate long-term development lines.

Security fixes are integrated into the main branch and released as updates for supported SUPLA device software components, including, where applicable:

- SUPLA device SDK,
- firmware components based on `supla-device`,
- supported firmware lines used by SUPLA-compatible devices,
- libraries and code maintained in this repository.

For supported devices and firmware lines, security fixes are normally delivered as complete firmware or software updates, typically through OTA updates where supported by the device.

Older source snapshots, historical releases, unofficial builds, forks, or modified firmware versions are not supported as separate maintenance branches. Users, integrators, and device manufacturers should update to the latest security release made available for their supported component, firmware line, or device software.

## Partner Devices

Some SUPLA-compatible devices using `supla-device` are manufactured and sold by third-party partners.

The device manufacturer is responsible for defining and communicating product-specific support periods, update availability, end-of-support dates, and user-facing update procedures.

SUPLA provides security fixes and update artifacts for supported `supla-device` components and firmware lines. The ability to deliver these updates to end devices depends on the partner’s hardware, firmware customization, and update mechanisms.

Partners integrating `supla-device` into commercial devices are responsible for maintaining product support policies and ensuring that supported devices can receive security updates for the declared support period.

## DIY and Community Use

`supla-device` can also be used in DIY and community projects.

DIY users are encouraged to keep their firmware, libraries, and device software updated to the latest available security release.

Security support for DIY builds, custom hardware, modified firmware, forks, and self-hosted deployments is provided on a best-effort basis unless a separate commercial support agreement applies.

## Security Model and Platform Considerations

`supla-device` supports multiple platforms with different security capabilities. 
The effective security properties of a device depend on the selected platform, build configuration, and deployment model.

### Production deployments

For commercial and production devices, the recommended configuration is based on ESP-IDF with hardware-supported security features, including:

- flash encryption,
- encrypted NVS storage,
- secure OTA update mechanisms.

When properly configured, these features provide protection of stored credentials against physical access and firmware extraction.

### Arduino and DIY deployments

In Arduino-based builds (ESP8266 and ESP32 Arduino), configuration data (such as WiFi credentials, API tokens, and device keys) may be stored in plaintext (e.g. LittleFS).

These environments typically do not provide a reliable root-of-trust or secure storage by default, and enabling hardware security features (such as flash encryption) is not commonly used due to complexity and deployment risks.

As a result:

- devices built using Arduino should not be considered resistant to physical access attacks,
- stored credentials may be recoverable by an attacker with direct access to device memory.

These limitations are inherent to the selected platform and deployment model.

### Provisioning Interfaces and HTTP Configuration Server

In Arduino-based builds (ESP8266 and ESP32 Arduino), the optional configuration interface is intended primarily for DIY, development, testing, and device provisioning scenarios.

On resource-constrained targets such as ESP8266, HTTPS/TLS protection for the embedded configuration server is generally not feasible due to RAM, flash, and CPU limitations. As a result, the configuration interface may operate over plain HTTP.

Current Arduino-based implementations may expose the HTTP configuration interface without authentication while a temporary provisioning mode is active. This behavior is a limitation of the current Arduino support path and is not considered a production-grade security model.

For this reason, Arduino-based deployments should not be considered suitable for hostile, untrusted, or publicly exposed network environments.

The recommended deployment model is:

- the configuration Access Point (AP) is started only after explicit local user action (for example, a hardware button press or equivalent local recovery mechanism),
- the AP is enabled temporarily for provisioning purposes only,
- the AP automatically shuts down after a limited time,
- the HTTP configuration server is active only while the provisioning mode is enabled.

The framework includes development and fallback options that may allow:

- automatic startup of a configuration AP on unconfigured devices,
- operation of the HTTP configuration server during normal LAN operation.

These modes are intended for development, debugging, testing, or recovery scenarios and are not recommended for commercial or production deployments.

Integrators and device manufacturers are responsible for selecting an appropriate provisioning model and disabling development-oriented configuration modes in production firmware.

The framework may emit runtime diagnostic and self-test warnings when insecure configuration patterns are detected.

Commercial and production deployments are expected to use ESP-IDF-based configurations with platform-supported security features and controlled provisioning flows.

### Responsibility of integrators

Integrators and device manufacturers are responsible for selecting an appropriate platform and enabling relevant security features for their use case.

For commercial products, use of ESP-IDF with hardware-backed security is strongly recommended.

## Reporting a Vulnerability

If you discover a security vulnerability in this repository, please report it
privately and **do not open a public GitHub issue**.

### How to report
- Email: security@supla.org
- Include as much detail as possible:
  - affected component or file
  - steps to reproduce
  - potential impact
  - any proof of concept (if available)

## Disclosure Process

After receiving a report:
1. We will acknowledge the report.
2. We will investigate and assess the impact.
3. If confirmed, we will work on a fix.
4. We may publish a security advisory after a fix is available.

Please allow reasonable time for us to investigate before any public disclosure.

## Security Updates

Security updates for `supla-device` may be delivered through one or more of the following channels:

- source code updates in this repository,
- SDK or library releases,
- firmware releases,
- OTA updates,
- partner-specific update packages.

Users, integrators, and device manufacturers should install or distribute security updates made available for their supported firmware line, device software, or device integration.

For devices manufactured by third-party partners, update availability and end-user instructions may depend on the manufacturer, model, hardware revision, firmware version, and update channel.

