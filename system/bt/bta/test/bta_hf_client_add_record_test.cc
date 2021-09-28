#include <base/logging.h>
#include <gtest/gtest.h>

#include "bta/hf_client/bta_hf_client_sdp.cc"
#include "btif/src/btif_hf_client.cc"

static uint16_t gVersion;

void LogMsg(uint32_t trace_set_mask, const char* fmt_str, ...) {}
bool SDP_AddProtocolList(uint32_t handle, uint16_t num_elem,
                         tSDP_PROTOCOL_ELEM* p_elem_list) {
  return false;
}
bool SDP_AddServiceClassIdList(uint32_t handle, uint16_t num_services,
                               uint16_t* p_service_uuids) {
  return false;
}
bool SDP_AddProfileDescriptorList(uint32_t handle, uint16_t profile_uuid,
                                  uint16_t version) {
  gVersion = version;
  return false;
}
bool SDP_AddAttribute(uint32_t handle, uint16_t attr_id, uint8_t attr_type,
                      uint32_t attr_len, uint8_t* p_val) {
  return false;
}
bool SDP_AddUuidSequence(uint32_t handle, uint16_t attr_id, uint16_t num_uuids,
                         uint16_t* p_uuids) {
  return false;
}

class BtaHfClientAddRecordTest : public ::testing::Test {
 protected:
  void SetUp() override { gVersion = 0; }

  void TearDown() override {}
};

TEST_F(BtaHfClientAddRecordTest, test_hf_client_add_record) {
  tBTA_HF_CLIENT_FEAT features = BTIF_HF_CLIENT_FEATURES;
  uint32_t sdp_handle = 0;
  uint8_t scn = 0;

  osi_property_set("persist.bluetooth.hfpclient.sco_s4_supported", "true");
  bta_hf_client_add_record("Handsfree", scn, features, sdp_handle);
  EXPECT_EQ(gVersion, 0x0107);
  sdp_handle++;
  scn++;
  osi_property_set("persist.bluetooth.hfpclient.sco_s4_supported", "false");
  bta_hf_client_add_record("Handsfree", scn, features, sdp_handle);
  EXPECT_EQ(gVersion, 0x0106);
}
