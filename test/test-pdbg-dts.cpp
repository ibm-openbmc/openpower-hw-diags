#include <fcntl.h>

#include <hei_main.hpp>
#include <test/sim-hw-access.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include <limits>
#include <vector>

#include "gtest/gtest.h"

TEST(PDBG, PdbgDtsTest1)
{
    using namespace util::pdbg;
    TARGETING::utils::targetingInit();

    trace::inf("retrieving hub targets.");
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);

    // Find hub at position 0
    TARGETING::TargetPtr hub0 = nullptr;
    for (auto hub : hubList)
    {
        if (getChipPos(hub) == 0)
        {
            hub0 = hub;
            break;
        }
    }
    EXPECT_NE(nullptr, hub0);
    uint32_t attr = hub0->getAttr<TARGETING::ATTR_CHIP_ID>();
    trace::inf("Chip ID: %u", attr);
    EXPECT_EQ(attr, 0);

    // Find hub at position 1
    TARGETING::TargetPtr hub1 = nullptr;
    for (auto hub : hubList)
    {
        if (getChipPos(hub) == 1)
        {
            hub1 = hub;
            break;
        }
    }
    EXPECT_NE(nullptr, hub1);
    attr = hub1->getAttr<TARGETING::ATTR_CHIP_ID>();
    trace::inf("Chip ID: %u", attr);
    EXPECT_EQ(attr, 1);
}

TEST(PDBG, PdbgDtsTest2)
{
    using namespace util::pdbg;
    TARGETING::utils::targetingInit();

    trace::inf("retrieving dimm targets.");
    TARGETING::TargetPtrList dimmList =
        TARGETING::utils::getTargets(TARGETING::TYPE_DIMM);

    // Find dimm at position 0
    TARGETING::TargetPtr dimm0 = nullptr;
    for (auto dimm : dimmList)
    {
        if (getChipPos(dimm) == 0)
        {
            dimm0 = dimm;
            break;
        }
    }
    EXPECT_NE(nullptr, dimm0);
}

TEST(util_pdbg, getParentChip)
{
    using namespace util::pdbg;
    TARGETING::utils::targetingInit();

    // Get a hub chip
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    auto hubChip = hubList[0];
    EXPECT_NE(nullptr, hubChip);

    // Get an OMI unit from the hub chip
    auto omiUnit = getChipUnit(hubChip, TARGETING::TYPE_OMI, 5);
    EXPECT_NE(nullptr, omiUnit);

    EXPECT_EQ(hubChip, getParentChip(hubChip)); // get self
    EXPECT_EQ(hubChip, getParentChip(omiUnit)); // get unit

    // Get OCMB chips
    TARGETING::TargetPtrList ocmbList =
        TARGETING::utils::getTargets(TARGETING::TYPE_OCMB_CHIP);
    auto ocmbChip = ocmbList[0];
    auto memPortUnit = getChipUnit(ocmbChip, TARGETING::TYPE_MEM_PORT, 0);

    EXPECT_EQ(ocmbChip, getParentChip(ocmbChip)); // get self
    EXPECT_EQ(ocmbChip,
              getParentChip(memPortUnit));        // get unit
}

TEST(util_pdbg, getChipUnit)
{
    using namespace util::pdbg;
    TARGETING::utils::targetingInit();

    // Get a hub chip
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    auto hubChip = hubList[0];
    EXPECT_NE(nullptr, hubChip);

    auto omiUnitPos = 5;

    // Get the unit and verify.
    auto omiUnit = getChipUnit(hubChip, TARGETING::TYPE_OMI, omiUnitPos);
    EXPECT_NE(nullptr, omiUnit);

    // Expect an exception when passing a unit instead of a chip.
    EXPECT_THROW(getChipUnit(omiUnit, TARGETING::TYPE_OMI, omiUnitPos),
                 std::logic_error);

    // Expect an exception when passing a chip type.
    EXPECT_THROW(getChipUnit(hubChip, TARGETING::TYPE_HUB_CHIP, omiUnitPos),
                 std::out_of_range);

    // Expect an exception when passing a unit type not on the target chip.
    EXPECT_THROW(getChipUnit(hubChip, TARGETING::TYPE_MEM_PORT, omiUnitPos),
                 std::out_of_range);

    // Expect a nullptr if the target is not found.
    EXPECT_EQ(nullptr, getChipUnit(hubChip, TARGETING::TYPE_OMI, 100));

    // Test with OCMB chip if available
    TARGETING::TargetPtrList ocmbList =
        TARGETING::utils::getTargets(TARGETING::TYPE_OCMB_CHIP);
    auto ocmbChip = ocmbList[0];
    auto memPortUnit = getChipUnit(ocmbChip, TARGETING::TYPE_MEM_PORT, 0);
    EXPECT_NE(nullptr, memPortUnit);
}

TEST(util_pdbg, getScom)
{
    using namespace util::pdbg;
    TARGETING::utils::targetingInit();

    // Get a hub chip
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    auto hubChip = hubList[0];
    EXPECT_NE(nullptr, hubChip);

    // Get OCMB and OMI targets
    TARGETING::TargetPtrList ocmbList =
        TARGETING::utils::getTargets(TARGETING::TYPE_OCMB_CHIP);
    auto ocmbChip = ocmbList[0];
    auto omiUnit = getChipUnit(hubChip, TARGETING::TYPE_OMI, 5);

    sim::ScomAccess& scom = sim::ScomAccess::getSingleton();
    scom.flush();
    scom.add(hubChip, 0x11111111, 0x0011223344556677);
    scom.error(ocmbChip, 0x22222222);

    int rc = 0;
    uint64_t val = 0;

    // Test good path.
    rc = getScom(hubChip, 0x11111111, val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(0x0011223344556677, val);

    // Test address that has not been added to ScomAccess.
    rc = getScom(hubChip, 0x33333333, val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(0, val);

    // Test SCOM error.
    rc = getScom(ocmbChip, 0x22222222, val);
    EXPECT_EQ(1, rc);

    // Test non-chip target.
    EXPECT_DEATH({ getScom(omiUnit, 0x11111111, val); }, "");
}

TEST(util_pdbg, getCfam)
{
    using namespace util::pdbg;
    TARGETING::utils::targetingInit();

    // Get a hub chip
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    auto hubChip = hubList[0];
    EXPECT_NE(nullptr, hubChip);

    auto omiUnit = getChipUnit(hubChip, TARGETING::TYPE_OMI, 5);

    sim::CfamAccess& cfam = sim::CfamAccess::getSingleton();
    cfam.flush();
    cfam.add(hubChip, 0x11111111, 0x00112233);
    cfam.error(hubChip, 0x22222222);

    int rc = 0;
    uint32_t val = 0;

    // Test good path.
    rc = getCfam(hubChip, 0x11111111, val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(0x00112233, val);

    // Test address that has not been added to CfamAccess.
    rc = getCfam(hubChip, 0x33333333, val);
    EXPECT_EQ(0, rc);
    EXPECT_EQ(0, val);

    // Test CFAM error.
    rc = getCfam(hubChip, 0x22222222, val);
    EXPECT_EQ(1, rc);

    // Test non-chip target.
    EXPECT_DEATH({ getCfam(omiUnit, 0x11111111, val); }, "");
}

TEST(util_pdbg, getActiveChips)
{
    using namespace util::pdbg;
    TARGETING::utils::targetingInit();

    std::vector<libhei::Chip> chips;
    getActiveChips(chips);

    trace::inf("chips size: %u", chips.size());
    EXPECT_EQ(2, chips.size());

    /* TODO: There is an issue with the getActiveChips() function that only
     *       seems to exist in simulation. For some reason, the OCMBs do not
     *       show up as PDBG_TARGET_ENABLED. If we remove that check, this test
     *       case works as expected. However, we don't want to do that in
     *       production code.  Instead, we'll need to determine why the OCMBs
     *       are not enabled in CI test and then reenable this test case.
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    auto hub0 = hubList[0];
    auto hub1 = hubList[1];

    sim::ScomAccess& scom = sim::ScomAccess::getSingleton();
    scom.flush();

    // Mask off hub0 mcc0 channel 1. The connected OCMB should be removed from
    // the list.
    scom.add(hub0, 0x08011842, 0x0780000000000000);

    // Mask off one or two attentions, but not all, on hub0 mcc2. None of the
    // connected OCMBs should be removed from the list.
    scom.add(hub0, 0x08011C42, 0x5280000000000000);

    // Mask off hub1 mcc7 channel 0. The connected OCMB should be removed from
    // the list.
    scom.add(hub1, 0x09011E42, 0x7800000000000000);

    // Mask off hub1 mcc5 channels 0 and 1. Both the connected OCMBs should be
    // removed from the list.
    scom.add(hub1, 0x09011A42, 0x7f80000000000000);

    std::vector<libhei::Chip> chips;
    getActiveChips(chips);

    // In total there should be 14 chips with 2 hub chips, 7 OCMBs on hub0,
    // and 5 OCMBs on hub1.

    trace::inf("chips size: %u", chips.size());
    EXPECT_EQ(14, chips.size());
    */
}
