# ESP-IDF custom local web pages

`Supla::EspIdfWebServer` lets a device add pages to the local configuration
web server without adding device-specific fields to the shared
`HtmlElement` list.

## Registration

Create a page description whose storage belongs to the device and register its
address before the web server is started:

```cpp
static esp_err_t installerGet(httpd_req_t *req, void *userData) {
  // Render the page and return ESP_OK.
  return ESP_OK;
}

static esp_err_t installerPost(
    httpd_req_t *req,
    const Supla::EspIdfWebServer::CustomPostRequest &postRequest,
    void *userData) {
  char value[64] = {};
  if (postRequest.getValue("example", value, sizeof(value))) {
    // Apply the device-specific setting from value.
  }
  // Render the response using req. Body reading, CSRF validation and URL
  // decoding are handled by EspIdfWebServer.
  return ESP_OK;
}

static Supla::EspIdfWebServer::CustomPage installerPage = {
    "/installer_settings",
    installerGet,
    installerPost,
    nullptr,
    Supla::EspIdfWebServer::CustomPageFactoryDefaultPolicy::RedirectToSetup,
};

Supla::EspIdfWebServer webServer;
webServer.registerCustomPage(&installerPage);
webServer.start();
```

The registration is non-owning: the `CustomPage` object, its URI, callbacks and
`userData` must remain valid for the lifetime of the web server. A page may
provide only GET, only POST, or both handlers. Duplicate URI/method pairs,
including collisions with standard routes, are rejected. Registration after
`start()` is rejected and logged. The registration list is retained across
`stop()`/`start()` cycles, while no page is served while the server is stopped.

The device still supplies the POST callback because only the device knows how
to apply its settings and render its response. It does not implement HTTP
body parsing: `EspIdfWebServer` performs that common part and exposes fields
through `CustomPostRequest`.

## Authorization and CSRF

Pages are served only while the device is in local config mode. In HTTPS mode,
when a password is configured, `EspIdfWebServer` calls the existing
`ensureAuthorized()` before
invoking a callback; an unauthorized request follows the existing redirect to
`/login`. When no password is configured, the default
`RedirectToSetup` policy redirects to `/setup`. A page that explicitly uses
`AllowWithoutPassword` can be entered in factory-default state, like `/setup`.

Every custom POST body is read and validated by the web server before the POST
callback is called. The body must contain the existing `csrf` token; invalid
tokens return HTTP 403. Body read timeouts return HTTP 408 and malformed
requests return HTTP 400. The callback receives a `CustomPostRequest`; use its
`getValue()` method to obtain URL-decoded fields instead of parsing the body.
The output buffer must fit the decoded value and its null terminator; URL
encoding overhead does not count towards this limit.
The request object is valid only while the callback runs. Bodies of 8192 bytes
or more are rejected with HTTP 413 to keep the shared server's memory use
bounded. A form can obtain the token with
`Supla::WebServer::Instance()->getCsrfToken()`.

In HTTPS mode custom pages are registered on the HTTPS server. The existing
HTTP listener remains a redirect to HTTPS. In HTTP-only mode they are served by
the HTTP listener without password or session checks, regardless of the
factory-default policy or whether a password is stored. CSRF validation still
applies to POST requests.

The HTTPD `max_uri_handlers` value is calculated from the standard routes plus
the number of registered custom GET/POST handlers, so custom pages do not
exhaust the handler table.
