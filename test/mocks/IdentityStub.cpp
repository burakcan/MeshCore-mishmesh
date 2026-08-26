// Ed25519/Curve25519 stubs so host tests can link the real Mesh/BaseChatMesh without
// pulling in rweather/Crypto. Signatures and shared secrets are fixed patterns; only
// tests that never exercise real crypto (contact-table layout, routing) may use these.
#include <Identity.h>
#include <string.h>

namespace mesh {

Identity::Identity() { memset(pub_key, 0, sizeof(pub_key)); }

LocalIdentity::LocalIdentity() { memset(pub_key, 0, sizeof(pub_key)); memset(prv_key, 0, sizeof(prv_key)); }

bool Identity::verify(const uint8_t* sig, const uint8_t* message, int msg_len) const {
  return true;
}

void LocalIdentity::sign(uint8_t* sig, const uint8_t* message, int msg_len) const {
  memset(sig, 0x5A, SIGNATURE_SIZE);
}

void LocalIdentity::calcSharedSecret(uint8_t* secret, const uint8_t* other_pub_key) const {
  memset(secret, 0x11, PUB_KEY_SIZE);
}

}
