#include <gtest/gtest.h>
#include "FakeContactsService.h"
#include "FakeDisplayDriver.h"
#include <mishmesh/core/AppletHost.h>
#include <mishmesh/applets/RepeaterTelemetryApplet.h>
#include <mishmesh/applets/RepeaterNeighborsApplet.h>
#include <cstring>

TEST(RepeaterTelem, RequestsOnOpenThenConsumesResult) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterTelemetryApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterTelemetryApplet());
  host.loop(1);
  EXPECT_EQ(FakeContactsService::keyStr(pk), svc.lastTelemetryReq);   // requested on open
  EXPECT_FALSE(mishmesh::repeaterTelemetryApplet().doneForTest());

  svc.telem.valid = true; svc.telem.count = 1;
  svc.telem.fields[0].channel = 1; svc.telem.fields[0].type = 0; svc.telem.fields[0].value = 3.9f;
  svc.telemSeq++;
  host.loop(300);
  EXPECT_TRUE(mishmesh::repeaterTelemetryApplet().doneForTest());     // result consumed
}

TEST(RepeaterNeighbors, FiresCliAndRendersLines) {
  FakeContactsService svc; FakeDisplayDriver d;
  mishmesh::AppletContext ctx; ctx.contacts = &svc;
  mishmesh::AppletHost host(&d, ctx);
  uint8_t pk[6] = {'R','E','P','0','0','1'};
  mishmesh::repeaterNeighborsApplet().setTarget(pk, "Rep1");
  host.setRoot(&mishmesh::repeaterNeighborsApplet());
  host.loop(1);
  EXPECT_EQ("neighbors", svc.lastCli);
  svc.simulateCliReply(pk, "AABBCCDD:120:10\nEEFF0011:45:8");
  host.loop(300);
  int hits = 0;
  for (int i = 0; i < mishmesh::repeaterNeighborsApplet().lineCountForTest(); i++) {
    std::string l = mishmesh::repeaterNeighborsApplet().lineForTest(i);
    if (l == "AABBCCDD:120:10" || l == "EEFF0011:45:8") hits++;
  }
  EXPECT_EQ(2, hits);
}

int main(int argc, char** argv) { ::testing::InitGoogleTest(&argc, argv); return RUN_ALL_TESTS(); }
