#pragma once

class QObject;

namespace whilefresh::recognition {

// Debug-only startup check that HTTPS works on the current platform. Sends an unauthenticated
// GET to the DeepSeek API and logs the outcome with the prefix "WhileFresh tls-smoke:".
// Any HTTP status (401 expected) proves the TLS handshake succeeded.
void startTlsSmokeTest(QObject *parent);

}
