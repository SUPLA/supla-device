# sd4linux HTTP Source

This example requires sd4linux built with HTTP source support. On Debian based
distributions install the CURL development package before building:

```sh
sudo apt install libcurl4-openssl-dev
```

If sd4linux is built without CURL, `source.type: HTTP` is unavailable and the
configuration will be rejected at startup.

`HTTP` source fetches a remote HTTP or HTTPS response body and passes it as text
to the existing parser pipeline. It does not parse JSON by itself; use
`parser.type: Json` and existing parsed channel mappings for that.

Supported in this PoC:

- `method: GET`
- static headers from `headers`
- `auth.type: none`
- `auth.type: bearer_file`
- `refresh_time_ms`
- `timeout_ms`
- `expiration_time_sec`
- `max_body_size_bytes`

Example:

```yaml
sources:
  st_washer:
    type: HTTP
    method: GET
    url: "https://api.smartthings.com/v1/devices/DEVICE_ID/status"
    headers:
      Accept: "application/json"
    auth:
      type: bearer_file
      token_file: "/etc/supla/smartthings-token"
    refresh_time_ms: 30000
    timeout_ms: 10000
    expiration_time_sec: 180
    max_body_size_bytes: 1048576

parsers:
  st_washer_json:
    type: Json
    source: st_washer
    refresh_time_ms: 1000

channels:
  - type: BinaryParsed
    parser: st_washer_json
    state: "/components/main/samsungce.washerOperatingState/operatingState/value"
    state_on_values: ["running"]
```

The source keeps the last successful response body in memory. Response bodies
are limited by `max_body_size_bytes`; the default is 1048576 bytes. Larger
responses are rejected during download and do not replace the previous cached
body. Calls before `refresh_time_ms` expires return the cached body and do not
issue another HTTP request. Failed requests keep the previous cached body. When
the cached body is older than `expiration_time_sec`, channels using this source
can report offline through the existing `isConnected()` path. Set
`expiration_time_sec: 0` to disable expiration.

Bearer tokens are read from `token_file`, trimmed, and sent as:

```text
Authorization: Bearer <token>
```

Inline tokens in YAML are intentionally unsupported. The token and
`Authorization` header are not logged.

## SmartThings token and device id

Create a SmartThings Personal Access Token at:

```text
https://account.smartthings.com/tokens
```

For this read-only status example, select scopes that allow reading devices and
device state/status. Do not put the token in YAML. Save it in a separate file:

```sh
SUPLA_USER=supla
SUPLA_GROUP=supla

sudo install -d -m 0750 -o "${SUPLA_USER}" -g "${SUPLA_GROUP}" /etc/supla
printf "%s\n" "PASTE_TOKEN_HERE" \
  | sudo install -m 0640 -o "${SUPLA_USER}" -g "${SUPLA_GROUP}" \
      /dev/stdin /etc/supla/smartthings-token
```

If your sd4linux process runs as a different user, adjust the owner/group above
so that the process can read `/etc/supla/smartthings-token`.

Device id can be found on: https://my.smartthings.com/advanced/devices

List SmartThings devices and find the washer `deviceId`:

```sh
SMARTTHINGS_TOKEN="$(tr -d '\r\n' < /etc/supla/smartthings-token)"

curl -sS \
  -H "Authorization: Bearer ${SMARTTHINGS_TOKEN}" \
  -H "Accept: application/json" \
  "https://api.smartthings.com/v1/devices"
```

Use the returned `deviceId` value in place of `DEVICE_ID` in
`smartthings_washer.yaml`. You can also filter the output with `jq`:

```sh
curl -sS \
  -H "Authorization: Bearer ${SMARTTHINGS_TOKEN}" \
  -H "Accept: application/json" \
  "https://api.smartthings.com/v1/devices" \
  | jq -r '.items[] | "\(.label // .name)\t\(.deviceId)"'
```

Before using the SmartThings example, test the selected device status manually:

```sh
curl -H "Accept: application/json" \
  -H "Authorization: Bearer ${SMARTTHINGS_TOKEN}" \
  "https://api.smartthings.com/v1/devices/DEVICE_ID/status"
```

Choose `refresh_time_ms` carefully. Provider APIs may enforce rate limits; this
PoC only retries after the configured refresh interval and does not implement
provider-specific backoff.
