#include <attn/attn-event.hpp>
#include <attn/attn_common.hpp>
#include <attn/attn_config.hpp>
#include <attn/attn_handler.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include "gtest/gtest.h"

namespace attn
{
// these are in the attn_lib but not all exposed via headers
int handleSpecial(Event* i_event);
} // namespace attn

using namespace attn;
using namespace util::pdbg;

/** @brief global function to be called back. */
int handleAttention(Event* attention)
{
    int rc = RC_SUCCESS;
    if (attention != nullptr)
    {
        return rc;
    }
    else
    {
        return RC_NOT_HANDLED;
    }
}

// Global variables for UT #1 and UT#2.
// Attention type
Event::AttentionType gType = Event::AttentionType::Special;
// pointer to handler callback function
int (*gHandler)(Event*) = &(handleSpecial);
const AttentionFlag gAttnFlag = AttentionFlag::enBreakpoints;

// Start preparation for UT case #1.

// Global variables for UT #1
const uint32_t gPos = 1;

/** @brief Fixture class for TEST_F(). */
class EventTestPos : public testing::Test
{
  public:
    EventTestPos() {}

    void SetUp()
    {
        TARGETING::utils::targetingInit();

        // Get hub1
        TARGETING::TargetPtrList hubList =
            TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
        target = nullptr;
        for (auto hub : hubList)
        {
            if (util::pdbg::getChipPos(hub) == gPos)
            {
                target = hub;
                break;
            }
        }
        EXPECT_NE(nullptr, target);

        config = new Config;
        EXPECT_EQ(true, config->getFlag(gAttnFlag));

        pAttn = std::make_unique<Event>(Event(gType, gHandler, target, config));
    }

    void TearDown()
    {
        delete config;
    }

    std::unique_ptr<Event> pAttn;
    Config* config = nullptr;
    TARGETING::TargetPtr target = nullptr;
};

TEST_F(EventTestPos, TestAttnTargetPos)
{
    EXPECT_EQ(0, pAttn->getPriority());
    EXPECT_EQ(RC_SUCCESS, pAttn->handle());

    // Verify the global target_tmp.
    EXPECT_NE(nullptr, target);
    uint32_t attr = util::pdbg::getChipPos(target);
    EXPECT_EQ(gPos, attr);

    // Verify the target in Attention object.
    TARGETING::TargetPtr target_tmp = pAttn->getTarget();
    EXPECT_NE(nullptr, target_tmp);
    attr = util::pdbg::getChipPos(target_tmp);
    EXPECT_EQ(gPos, attr);

    // Verify the config in Attention object.
    Config* config_tmp = pAttn->getConfig();
    EXPECT_EQ(true, config_tmp->getFlag(gAttnFlag));
    config_tmp->clearFlag(gAttnFlag);
    EXPECT_EQ(false, config_tmp->getFlag(gAttnFlag));
    config_tmp->setFlag(gAttnFlag);
    EXPECT_EQ(true, config_tmp->getFlag(gAttnFlag));
}

// Start preparation for UT case #2.

// Global variables for UT #2
const uint32_t gChipId = 0x20da; // Chip ID for proc0.

/** @brief Fixture class for TEST_F(). */
class EventTestProc : public testing::Test
{
  public:
    EventTestProc() {}

    void SetUp()
    {
        TARGETING::utils::targetingInit();
        target = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP)[0];

        EXPECT_NE(nullptr, target);

        attr = util::pdbg::getTrgtType(target);
        EXPECT_EQ(TARGETING::TYPE_HUB_CHIP, attr);

        attr = target->getAttr<TARGETING::ATTR_CHIP_ID>();
        EXPECT_EQ(attr, gChipId);

        config = new Config;
        EXPECT_EQ(true, config->getFlag(gAttnFlag));

        pAttn = std::make_unique<Event>(Event(gType, gHandler, target, config));
    }

    void TearDown()
    {
        delete config;
    }

    std::unique_ptr<Event> pAttn;
    Config* config = nullptr;
    TARGETING::TargetPtr target = nullptr;
    uint32_t attr = std::numeric_limits<uint32_t>::max();
};

TEST_F(EventTestProc, TestEventProc)
{
    EXPECT_EQ(0, pAttn->getPriority());
    EXPECT_EQ(RC_SUCCESS, pAttn->handle());

    // Verify the target in Attention object.
    attr = std::numeric_limits<uint32_t>::max();
    TARGETING::TargetPtr target_tmp = pAttn->getTarget();
    EXPECT_NE(nullptr, target_tmp);
    attr = util::pdbg::getTrgtType(target_tmp);
    EXPECT_EQ(TARGETING::TYPE_HUB_CHIP, attr);

    attr = target_tmp->getAttr<TARGETING::ATTR_CHIP_ID>();
    EXPECT_EQ(attr, gChipId);

    // Verify the config in Attention object.
    Config* config_tmp = pAttn->getConfig();
    EXPECT_EQ(true, config_tmp->getFlag(gAttnFlag));
    config_tmp->clearFlag(gAttnFlag);
    EXPECT_EQ(false, config_tmp->getFlag(gAttnFlag));
    config_tmp->setFlag(gAttnFlag);
    EXPECT_EQ(true, config_tmp->getFlag(gAttnFlag));
}

TEST(AttnConfig, TestAttnConfig)
{
    Config* config = new Config();

    // Test clearFlagAll() function.
    config->clearFlagAll();
    EXPECT_EQ(false, config->getFlag(AttentionFlag::enVital));
    EXPECT_EQ(false, config->getFlag(AttentionFlag::enCheckstop));
    EXPECT_EQ(false, config->getFlag(AttentionFlag::enTerminate));
    EXPECT_EQ(false, config->getFlag(AttentionFlag::enBreakpoints));
    // The dfltTi flag is not impacted.
    EXPECT_EQ(false, config->getFlag(AttentionFlag::dfltTi));
    EXPECT_EQ(false, config->getFlag(AttentionFlag::enClrAttnIntr));

    // Test setFlagAll() function.
    config->setFlagAll();
    EXPECT_EQ(true, config->getFlag(AttentionFlag::enVital));
    EXPECT_EQ(true, config->getFlag(AttentionFlag::enCheckstop));
    EXPECT_EQ(true, config->getFlag(AttentionFlag::enTerminate));
    EXPECT_EQ(true, config->getFlag(AttentionFlag::enBreakpoints));
    // The dfltTi flag is not impacted.
    EXPECT_EQ(false, config->getFlag(AttentionFlag::dfltTi));
    EXPECT_EQ(true, config->getFlag(AttentionFlag::enClrAttnIntr));

    // Test setFlag() and getFlag() functions.
    // Only test one flag.
    config->clearFlagAll();
    config->setFlag(enVital);
    EXPECT_EQ(true, config->getFlag(enVital));

    delete config;
}
