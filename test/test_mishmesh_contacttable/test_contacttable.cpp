// Covers the seam between mishmesh's contact list and BaseChatMesh's contact table.
// meshcore v1.17 reserved the first MAX_ANON_CONTACTS slots of contacts[] for anon
// requests and redefined getNumContacts() to exclude them, which silently shifted every
// mishmesh list that paired that count with a raw array index: the first rows rendered
// blank (empty name, type ADV_TYPE_NONE) and the tail fell off. Nothing in the type
// system moved, so only a test that walks a populated table catches it.
#include <gtest/gtest.h>
#include <helpers/BaseChatMesh.h>
#include <helpers/AdvertDataHelpers.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/SimpleMeshTables.h>
#include <string.h>

namespace {

class StubRTC : public mesh::RTCClock {
public:
  uint32_t getCurrentTime() override { return 1000; }
  void setCurrentTime(uint32_t time) override { }
};

class StubMillis : public mesh::MillisecondClock {
public:
  unsigned long getMillis() override { return 0; }
};

class StubRNG : public mesh::RNG {
public:
  void random(uint8_t* dest, size_t sz) override { memset(dest, 0, sz); }
};

class StubRadio : public mesh::Radio {
public:
  uint32_t getEstAirtimeFor(int len_bytes) override { return 0; }
  int recvRaw(uint8_t* bytes, int sz) override { return 0; }
  float packetScore(float snr, int packet_len) override { return 0; }
  bool startSendRaw(const uint8_t* bytes, int len) override { return true; }
  bool isSendComplete() override { return true; }
  void onSendFinished() override { }
  bool isInRecvMode() const override { return true; }
  bool isReceiving() override { return false; }
};

// The smallest concrete BaseChatMesh that will stand up: every hook is a no-op, since
// these tests only exercise the contact table.
class TestMesh : public BaseChatMesh {
public:
  TestMesh(mesh::Radio& r, mesh::MillisecondClock& m, mesh::RNG& g, mesh::RTCClock& c,
           mesh::PacketManager& p, mesh::MeshTables& t) : BaseChatMesh(r, m, g, c, p, t) { }

  void onDiscoveredContact(ContactInfo&, bool, uint8_t, const uint8_t*) override { }
  ContactInfo* processAck(const uint8_t*) override { return nullptr; }
  void onContactPathUpdated(const ContactInfo&) override { }
  void onMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override { }
  void onCommandDataRecv(const ContactInfo&, mesh::Packet*, uint32_t, const char*) override { }
  void onSignedMessageRecv(const ContactInfo&, mesh::Packet*, uint32_t, const uint8_t*, const char*) override { }
  uint32_t calcFloodTimeoutMillisFor(uint32_t) const override { return 0; }
  uint32_t calcDirectTimeoutMillisFor(uint32_t, uint8_t) const override { return 0; }
  void onSendTimeout() override { }
  void onChannelMessageRecv(const mesh::GroupChannel&, mesh::Packet*, uint32_t, const char*) override { }
  uint8_t onContactRequest(const ContactInfo&, uint32_t, const uint8_t*, uint8_t, uint8_t*) override { return 0; }
  void onContactResponse(const ContactInfo&, const uint8_t*, uint8_t) override { }
};

struct Fixture {
  StubRadio radio;
  StubMillis millis;
  StubRNG rng;
  StubRTC rtc;
  StaticPoolPacketManager mgr{4};
  SimpleMeshTables tables;
  TestMesh mesh{radio, millis, rng, rtc, mgr, tables};

  void add(const char* name, uint8_t type, uint8_t seed) {
    ContactInfo c;
    memset(&c, 0, sizeof(c));
    memset(c.id.pub_key, seed, PUB_KEY_SIZE);   // distinct key per contact
    snprintf(c.name, sizeof(c.name), "%s", name);
    c.type = type;
    c.out_path_len = OUT_PATH_UNKNOWN;
    ASSERT_TRUE(mesh.addContact(c));
  }
};

}  // namespace

TEST(ContactTable, LogicalIndexZeroIsTheFirstRealContact) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);

  ASSERT_EQ(1, f.mesh.getNumContacts());
  const ContactInfo* c = f.mesh.getContactPtrByIdx(0);
  ASSERT_NE(nullptr, c);
  // Pre-fix this was contacts[0] - a zeroed anon slot, which is what put a blank
  // header on the contact detail screen.
  EXPECT_STREQ("Alice", c->name);
  EXPECT_EQ(ADV_TYPE_CHAT, c->type);
}

TEST(ContactTable, EveryLogicalIndexResolvesToARealContact) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);
  f.add("Bob", ADV_TYPE_CHAT, 3);

  int n = f.mesh.getNumContacts();
  ASSERT_EQ(3, n);
  for (int i = 0; i < n; i++) {
    const ContactInfo* c = f.mesh.getContactPtrByIdx(i);
    ASSERT_NE(nullptr, c) << "index " << i;
    EXPECT_NE('\0', c->name[0]) << "blank name at index " << i;
    EXPECT_NE(ADV_TYPE_NONE, c->type) << "anon slot leaked at index " << i;
  }
}

TEST(ContactTable, LogicalIndexIsBounded) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);

  EXPECT_EQ(nullptr, f.mesh.getContactPtrByIdx(-1));
  EXPECT_EQ(nullptr, f.mesh.getContactPtrByIdx(f.mesh.getNumContacts()));
}

// The Repeaters tab: scan [0, getNumContacts()) and keep the matching types. This is
// the exact loop shape UITask uses, and the one that reported an empty tab.
TEST(ContactTable, FilteringByTypeFindsRepeaters) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);
  f.add("Bob", ADV_TYPE_CHAT, 3);

  int found = 0;
  const char* name = "";
  int n = f.mesh.getNumContacts();
  for (int i = 0; i < n; i++) {
    const ContactInfo* c = f.mesh.getContactPtrByIdx(i);
    if (c && c->type == ADV_TYPE_REPEATER) { found++; name = c->name; }
  }
  EXPECT_EQ(1, found);
  EXPECT_STREQ("Hilltop", name);
}

// getTotalContactSlots() counts the reserved slots, getNumContacts() does not. Pinning
// the difference documents why a raw index and a logical one are not interchangeable.
TEST(ContactTable, TotalSlotsIncludeTheReservedAnonSlots) {
  Fixture f;
  EXPECT_EQ(0, f.mesh.getNumContacts());
  EXPECT_EQ(MAX_ANON_CONTACTS, f.mesh.getTotalContactSlots());

  f.add("Alice", ADV_TYPE_CHAT, 1);
  EXPECT_EQ(1, f.mesh.getNumContacts());
  EXPECT_EQ(MAX_ANON_CONTACTS + 1, f.mesh.getTotalContactSlots());
}

// addContact() allocates a slot without checking for an existing pubkey, so callers
// must guard with lookupContactByPubKey() first. MyMesh::uiAddDiscovery skipped that
// and appended a second copy when adding a node that was already a contact.
TEST(ContactTable, AddContactDoesNotDedupeByPubKey) {
  Fixture f;
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);   // same pub_key
  EXPECT_EQ(2, f.mesh.getNumContacts()) << "addContact() is expected NOT to dedupe";
}

TEST(ContactTable, LookupByPubKeyFindsAnExistingContact) {
  Fixture f;
  f.add("Alice", ADV_TYPE_CHAT, 1);
  f.add("Hilltop", ADV_TYPE_REPEATER, 2);

  uint8_t key[PUB_KEY_SIZE];
  memset(key, 2, sizeof(key));
  const ContactInfo* found = f.mesh.lookupContactByPubKey(key, 6);
  ASSERT_NE(nullptr, found);
  EXPECT_STREQ("Hilltop", found->name);

  memset(key, 9, sizeof(key));   // never added
  EXPECT_EQ(nullptr, f.mesh.lookupContactByPubKey(key, 6));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
