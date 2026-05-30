#pragma once

#include <mishmesh/core/Applet.h>
#include <mishmesh/core/ContactsService.h>
#include <mishmesh/widgets/ListMenu.h>
#include <mishmesh/widgets/ScrollText.h>
#include <mishmesh/widgets/Card.h>

namespace mishmesh {

// Detail screen for a discovered (not-yet-added) node: a card header, a
// scrollable info panel (type / distance / last heard / GPS / public key), and a
// short action list whose primary action adds the node as a contact. Mirrors
// ContactDetailApplet's layout but for nodes that are not contacts yet.
class DiscoverDetailApplet : public Applet, public ListModel {
public:
  enum Action { Add, View, ACTION_COUNT };
private:
  AppletHost*      _host;
  ContactsService* _svc;
  AppServices*     _app;
  uint8_t          _pubkey[6];
  uint8_t          _fullKey[PUBKEY_LEN];
  char             _name[32];
  char             _displayName[44];
  char             _infoLine[48];
  uint8_t          _type;
  bool             _hasPath;
  uint8_t          _hops;
  uint32_t         _lastAdvert;
  bool             _hasLoc;
  int32_t          _gpsLat, _gpsLon;
  float            _distKm;
  ListMenu         _list;
  Card             _card;
  ScrollText       _details;
  bool             _viewing;

  void buildInfo();
public:
  DiscoverDetailApplet();
  void setTarget(const ContactView& v);     // snapshot the discovery to display
  void onStart(AppletContext& ctx) override;
  int  onRender(Canvas& c) override;
  bool onInput(InputEvent ev) override;
  int count() const override { return ACTION_COUNT; }
  const char* label(int i) const override;
};

DiscoverDetailApplet& discoverDetailApplet();

}  // namespace mishmesh
