# Contributing to supla-device

Thank you for your interest in contributing to **supla-device**.
This document describes the rules and guidelines for contributing code,
documentation, and ideas to the project.

## Table of contents
- [Contributor License Agreement (CLA)](#contributor-license-agreement-cla)
- [Project scope and goals](#project-scope-and-goals)
- [Supported platforms](#supported-platforms)
- [Repository structure](#repository-structure)
- [Reporting issues](#reporting-issues)
- [Feature requests](#feature-requests)
- [Development guidelines](#development-guidelines)
- [Documentation requirements](#documentation-requirements)
- [Pull request process](#pull-request-process)
- [Licensing](#licensing)

---

## Contributor License Agreement (CLA)

This project requires acceptance of a **Contributor License Agreement (CLA)**.

Before a pull request can be merged, the CLA must be accepted by the contributor.

Pull requests with an unaccepted CLA will be blocked automatically.

---

## Project scope and goals

`supla-device` is a core device-side library for the SUPLA ecosystem.
It is designed primarily as an **Arduino-compatible library**, with additional
supporting code for other platforms (ESP-IDF, Linux).

Key goals:
- compatibility with Arduino tooling,
- stability and backward compatibility,
- support for resource-constrained devices.

Changes should be made with long-term maintenance and portability in mind.

---

## Supported platforms

- **Arduino**
  - Arduino IDE
  - PlatformIO
  - ESP8266
  - ESP32
  - Arduino Mega (supported in a limited scope due to hardware constraints)

- **ESP-IDF**
  - ESP32

- **sd4linux**
  - Linux
  - macOS

---

## Repository structure

The repository follows the **Arduino library directory layout**.

Key rules:

- `src/`
  - must compile correctly as an Arduino library,
  - must remain compatible with Arduino IDE and PlatformIO,
  - contains code shared between Arduino and ESP-IDF where possible.

- Due to Arduino build system limitations:
  - all code in `src/` must be self-contained and compilable,
  - in some cases implementations are placed in `.h` files instead of `.cpp`
    to ensure correct compilation.

- `extras/`
  - contains platform-specific code and helpers,
  - may include ESP-IDF–specific or Linux-specific implementations,
  - code here must not break any build.

If you are unsure where new code should be placed, feel free to open pull
request and start a discussion there.

---

## Reporting issues

Before opening an issue:
1. Search existing issues (open and closed).
2. Verify that you are using a newest release.

When reporting a bug, please include:
- target environment (Arduino or ESP-IDF),
- board or device (e.g. ESP32, ESP8266, Arduino Mega),
- library version or commit hash,
- steps to reproduce the issue,
- logs or error messages if available.

---

## Feature requests

Feature requests are welcome for both Arduino and ESP-IDF use cases.

Please describe:
- the problem you are trying to solve,
- why existing functionality is insufficient,
- potential impact on memory usage and compatibility,
- whether the change affects Arduino, ESP-IDF, or both.

---

## Development guidelines

- Write portable C/C++ code where possible.
- Keep Arduino and ESP-IDF compatibility in mind.
- Avoid unnecessary dynamic memory allocation.
- Be mindful of RAM and flash usage.
- Prefer readable and explicit code over clever constructs.
- Avoid breaking public APIs without prior discussion.

---

## Documentation requirements

Documentation is considered part of the contribution.

If your change:
- adds functionality,
- changes existing behavior,
- introduces new configuration options,

you **must** update relevant documentation:
- README,
- files in `docs/`,
- example documentation if applicable,
- Doxygen in header files.

---

## Pull request process

1. Fork the repository and create a feature branch.
2. Make small, focused commits with clear commit messages.
3. Ensure the code builds for the affected platforms (Arduino and/or ESP-IDF).
4. Update documentation if required.
5. Open a pull request using the provided template.
6. Ensure the CLA check passes.

Each pull request should:
- address a single issue or feature,
- include a clear description of the changes,
- reference related issues if applicable.

---

## Licensing

By contributing to this repository, you agree that your contributions
will be licensed under the same license as the project.

Any third-party code must use a compatible license and be clearly documented.

---

Thank you for contributing to supla-device.
