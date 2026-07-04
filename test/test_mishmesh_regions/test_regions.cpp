#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/RepeaterRegionsApplet.h>
#include <cstring>

static void openAndFetch(mishmesh::AppletHost& host, FakeContactsService& svc, const uint8_t* pk) {
  mishmesh::repeaterRegionsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterRegionsApplet());
  host.loop(1);                              // fires "region list allowed"
  svc.simulateCliReply(pk, "aaa,bbb");
  host.loop(200);                            // parses allowed, fires "region list denied"
  svc.simulateCliReply(pk, "ccc");
  host.loop(400);                            // parses denied
}

TEST(RepeaterRegions, IsLoadingDuringFetch) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterRegionsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterRegionsApplet());
  host.loop(1);   // fires "region list allowed"; no reply yet -- still Loading
  EXPECT_TRUE(mishmesh::repeaterRegionsApplet().isLoadingForTest());
  EXPECT_EQ(0, mishmesh::repeaterRegionsApplet().regionCountForTest());
}

TEST(RepeaterRegions, SaveRowIsButton) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  openAndFetch(host, svc, pk);
  EXPECT_TRUE(mishmesh::repeaterRegionsApplet().saveRowIsButtonForTest());
}

TEST(RepeaterRegions, FetchesAllowedAndDenied) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  openAndFetch(host, svc, pk);
  auto& p = mishmesh::repeaterRegionsApplet();
  EXPECT_EQ(3, p.regionCountForTest());
  EXPECT_STREQ("aaa", p.regionNameForTest(0)); EXPECT_TRUE(p.regionAllowedForTest(0));
  EXPECT_STREQ("bbb", p.regionNameForTest(1)); EXPECT_TRUE(p.regionAllowedForTest(1));
  EXPECT_STREQ("ccc", p.regionNameForTest(2)); EXPECT_FALSE(p.regionAllowedForTest(2));
}

TEST(RepeaterRegions, SelectTogglesDenyf) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  openAndFetch(host, svc, pk);
  // Row 0 = "aaa" (allowed) -> Select fires denyf.
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ("region denyf aaa", svc.lastCli);
  svc.simulateCliReply(pk, "OK");
  host.loop(600);
  EXPECT_FALSE(mishmesh::repeaterRegionsApplet().regionAllowedForTest(0));   // optimistic flip
}

TEST(RepeaterRegions, SaveRowFiresRegionSave) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  openAndFetch(host, svc, pk);
  // Rows: 3 regions + Add + Delete + Save = index 5 is Save.
  for (int i = 0; i < 5; i++) host.dispatch(mishmesh::InputEvent::NavDown);
  host.dispatch(mishmesh::InputEvent::Select);
  EXPECT_EQ("region save", svc.lastCli);
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
