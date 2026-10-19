#pragma once

#include <WiFiClientSecure.h>

// Enable verification against the CA bundle compiled into the firmware
// (CONFIG_MBEDTLS_CERTIFICATE_BUNDLE_DEFAULT_FULL). The core offers no public
// switch for it: NetworkClientSecure::setCACertBundle() needs an external
// bundle blob, so this subclass flips the same two internals it would
// (bundle_attach_cb + _use_ca_bundle) and lets ssl_client fall back to the
// embedded bundle. Used by the webhooks and the GitHub updater.
class BundleVerifyingClient : public NetworkClientSecure {
 public:
  void useBuiltinCACertBundle() {
    attach_ssl_certificate_bundle(sslclient.get(), true);
    _use_ca_bundle = true;
  }
};
