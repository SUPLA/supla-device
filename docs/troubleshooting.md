# Troubleshooting

---

## Device does not appear in SUPLA Cloud

### Possible causes
- Device is not connected to SUPLA Server.
- Incorrect server address, GUID or AUTHKEY.
- Network connection is not working.
- TLS/SSL connection cannot be established.

### What to check
- Verify network connectivity (Wi-Fi or Ethernet).
- Confirm that GUID and AUTHKEY are correct and unchanged.
- Check device logs for connection or registration errors.
- Make sure the device can reach the SUPLA Server address.

---

## Device appears in SUPLA Cloud, but not in mobile app

### Possible causes
- The device has no registered Channels visible to mobile applications.
- The device or its Channels are not assigned to a Location accessible
  to the Access Identifier used by the mobile client.

### What to check
- Verify in SUPLA Cloud that the device has at least one registered Channel.
  Note that some Channel types are not displayed in mobile applications.
- Check that each Channel has a valid **Function** selected.
  Channels with function set to **"none"** are not shown in mobile apps.
- Check that **"Show on the Client’s devices"** option is enabled for the Channel.
- Check that the device is assigned to a Location.
- Verify that the Location is visible for the Access Identifier
  associated with the mobile application (client).

---

## Device connects to network but does not register

### Possible causes
- Channel order changed since the last registration (channel conflict).
- Channel type was changed (channel conflict).
- A channel was removed (channel conflict).
- Invalid server address.
- Invalid user credentials (e-mail).

### What to check
- Ensure channels are created in the same order as before.
- If channel structure changed, remove the device from SUPLA Cloud
  and register it again from scratch.
- Verify that the server address is correct.
- Verify that the user credentials are correct.

---

## Build fails after updating Arduino board packages or ESP-IDF

### Possible causes
- Breaking changes in a newly released board package or ESP-IDF version.
- Temporary incompatibility with the latest toolchain.

### What to check
- Try the previous stable version of the board package or ESP-IDF.
- Review build errors and warnings carefully.
- Check recent issues or releases for known incompatibilities.

When reporting the issue, always include:
- board package or ESP-IDF version,
- supla-device version or commit hash,
- full build log.

---

## Device runs out of memory (RAM)

### Possible causes
- TLS/SSL enabled on very small targets.
- Too many Channels or Elements.
- Insufficient heap on constrained devices (e.g. ESP8266, Arduino Mega).

### What to check
- Monitor free heap (if available on your platform).
- Disable TLS if memory is insufficient (for testing only).
- Reduce the number of Channels or simplify device logic.
- Consider using ESP32 or ESP-IDF for more complex devices.

---

## Device is unresponsive or freezes

### Possible causes
- Blocking operations in the main loop or `iterate()` callbacks.
- Network connection attempts blocking execution.
- Long-running code inside Element lifecycle methods.

### What to check
- Avoid blocking delays in device logic.
- Keep Element callbacks short and non-blocking.
- Be careful with operations executed on every `iterate()` call.

---

## Data is not saved or restored correctly

### Possible causes
- Storage not configured but required by some Elements.
- Incorrect storage offset or configuration.
- Storage device not available or failing.

### What to check
- Ensure persistent storage is configured when required.
- Verify storage implementation and wiring (if external).
- Check logs related to loading and saving state.

---

## Still stuck?

If none of the above helps:
- check existing issues in the repository,
- open a new issue and include detailed environment information,
- use the **Question / Help needed** issue template if the problem
  is not a bug.
- ask for help on the SUPLA community forum:
  - English forum (https://en-forum.supla.org/),
  - Polish forum (https://forum.supla.org/).

Providing logs and configuration details will significantly
speed up diagnosis.

