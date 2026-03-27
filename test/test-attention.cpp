#include <attn/attention.hpp>
#include <attn/attn_common.hpp>
#include <attn/attn_config.hpp>
#include <attn/attn_handler.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include "gtest/gtest.h"

namespace attn
{
// these are in the attn_lib but not all exposed via headers
int handleSpecial(Attention* i_attention);
} // namespace attn

using namespace attn;
using namespace util::pdbg;

/** @brief global function to be called back. */
int handleAttention(Attention* attention)
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
Attention::AttentionType gType = Attention::AttentionType::Special;
// pointer to handler callback function
int (*gHandler)(Attention*) = &(handleSpecial);
const AttentionFlag gAttnFlag = AttentionFlag::enBreakpoints;

// Start preparation for UT case #1.

// Global variables for UT #1
const char* gPosPath = "/proc0/pib/perv12";
const uint32_t gPos = 12;

/** @brief Fixture class for TEST_F(). */
class AttentionTestPos : public testing::Test
{
  public:
    AttentionTestPos() {}

    void SetUp()
    {
        pdbg_targets_init(nullptr);

        target = pdbg_target_from_path(nullptr, gPosPath);
        EXPECT_NE(nullptr, target);

        config = new Config;
        EXPECT_EQ(true, config->getFlag(gAttnFlag));

        pAttn = std::make_unique<Attention>(
            Attention(gType, gHandler, target, config));
    }

    void TearDown()
    {
        delete config;
    }

    std::unique_ptr<Attention> pAttn;
    Config* config = nullptr;
    pdbg_target* target = nullptr;
};

TEST_F(AttentionTestPos, TestAttnTargetPos)
{
    EXPECT_EQ(0, pAttn->getPriority());
    EXPECT_EQ(RC_SUCCESS, pAttn->handle());

    // Verify the global target_tmp.
    EXPECT_NE(nullptr, target);
    uint32_t attr = std::numeric_limits<uint32_t>::max();
    pdbg_target_get_attribute(target, "ATTR_FAPI_POS", 4, 1, &attr);
    EXPECT_EQ(gPos, attr);

    // Verify the target in Attention object.
    attr = std::numeric_limits<uint32_t>::max();
    pdbg_target* target_tmp = pAttn->getTarget();
    EXPECT_NE(nullptr, target_tmp);
    pdbg_target_get_attribute(target_tmp, "ATTR_FAPI_POS", 4, 1, &attr);
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
class AttentionTestProc : public testing::Test
{
  public:
    AttentionTestProc() {}

    void SetUp()
    {
        pdbg_targets_init(nullptr);
        target = getPrimaryHub();

        EXPECT_NE(nullptr, target);

        attr = getTrgtType(target);
        EXPECT_EQ(TYPE_PROC, attr);

        attr = std::numeric_limits<uint32_t>::max();
        pdbg_target_get_attribute(target, "ATTR_CHIP_ID", 4, 1, &attr);
        EXPECT_EQ(attr, gChipId);

        config = new Config;
        EXPECT_EQ(true, config->getFlag(gAttnFlag));

        pAttn = std::make_unique<Attention>(
            Attention(gType, gHandler, target, config));
    }

    void TearDown()
    {
        delete config;
    }

    std::unique_ptr<Attention> pAttn;
    Config* config = nullptr;
    pdbg_target* target = nullptr;
    uint32_t attr = std::numeric_limits<uint32_t>::max();
};

TEST_F(AttentionTestProc, TestAttentionProc)
{
    EXPECT_EQ(0, pAttn->getPriority());
    EXPECT_EQ(RC_SUCCESS, pAttn->handle());

    // Verify the target in Attention object.
    attr = std::numeric_limits<uint32_t>::max();
    pdbg_target* target_tmp = pAttn->getTarget();
    EXPECT_NE(nullptr, target_tmp);
    attr = getTrgtType(target_tmp);
    EXPECT_EQ(TYPE_PROC, attr);

    attr = std::numeric_limits<uint32_t>::max();
    pdbg_target_get_attribute(target_tmp, "ATTR_CHIP_ID", 4, 1, &attr);
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
