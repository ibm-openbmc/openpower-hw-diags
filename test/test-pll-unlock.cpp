#include <stdio.h>

#include <analyzer/plugins/plugin.hpp>
#include <analyzer/ras-data/ras-data-parser.hpp>
#include <hei_util.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include "gtest/gtest.h"

namespace analyzer
{
// Forward reference of filterRootCause
bool filterRootCause(AnalysisType i_type,
                     const libhei::IsolationData& i_isoData,
                     libhei::Signature& o_rootCause,
                     const RasDataParser& i_rasData);
} // namespace analyzer

using namespace analyzer;

static const auto nodeId =
    static_cast<libhei::NodeId_t>(libhei::hash<libhei::NodeId_t>("PLL_UNLOCK"));

// Sub-test #1 - single PLL unlock attention on proc 1, clock 1
TEST(PllUnlock, TestSet1)
{
    pdbg_targets_init(nullptr);

    libhei::Chip chip1{util::pdbg::getTrgt("/proc1"), P10_20};

    libhei::Signature sig11{chip1, nodeId, 0, 1, libhei::ATTN_TYPE_CHIP_CS};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig11);
    ServiceData sd{sig11, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig11)->resolve(sd);

    nlohmann::json j{};
    std::string s{};

    // Callout list
    j = sd.getCalloutList();
    s = R"([
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "P0",
        "Priority": "A"
    },
    {
        "Deconfigured": false,
        "EntityPath": [],
        "GuardType": "GARD_Unrecoverable",
        "Guarded": true,
        "LocationCode": "/proc1",
        "Priority": "A"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Callout Type": "Clock Callout",
        "Clock Type": "OSC_REF_CLOCK_1",
        "Priority": "medium_group_A"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": true,
        "Priority": "medium_group_A",
        "Target": "/proc1"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Sub-test #2 - PLL unlock attention on multiple procs and clocks. Isolating
//               only to proc 1 clock 0 PLL unlock attentions.
TEST(PllUnlock, TestSet2)
{
    pdbg_targets_init(nullptr);

    libhei::Chip chip0{util::pdbg::getTrgt("/proc0"), P10_20};
    libhei::Chip chip1{util::pdbg::getTrgt("/proc1"), P10_20};

    // PLL unlock signatures for each clock per processor.
    libhei::Signature sig00{chip0, nodeId, 0, 0, libhei::ATTN_TYPE_CHIP_CS};
    libhei::Signature sig01{chip0, nodeId, 0, 1, libhei::ATTN_TYPE_CHIP_CS};
    libhei::Signature sig10{chip1, nodeId, 0, 0, libhei::ATTN_TYPE_CHIP_CS};
    libhei::Signature sig11{chip1, nodeId, 0, 1, libhei::ATTN_TYPE_CHIP_CS};

    // Plugins for each processor.
    auto plugin = PluginMap::getSingleton().get(chip1.getType(), "pll_unlock");

    libhei::IsolationData isoData{};
    isoData.addSignature(sig00);
    isoData.addSignature(sig01);
    isoData.addSignature(sig10);
    isoData.addSignature(sig11);
    ServiceData sd{sig10, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    // Call the PLL unlock plugin.
    plugin(0, chip1, sd);

    nlohmann::json j{};
    std::string s{};

    // Callout list
    j = sd.getCalloutList();
    s = R"([
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "P0",
        "Priority": "H"
    },
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc0",
        "Priority": "M"
    },
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc1",
        "Priority": "M"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Callout Type": "Clock Callout",
        "Clock Type": "OSC_REF_CLOCK_0",
        "Priority": "high"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": false,
        "Priority": "medium",
        "Target": "/proc0"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": false,
        "Priority": "medium",
        "Target": "/proc1"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Sub-test #3 - PLL unlock on single OCMB.
TEST(PllUnlock, TestSet3)
{
    pdbg_targets_init(nullptr);

    libhei::Chip ocmb0{
        util::pdbg::getTrgt("/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"),
        ODYSSEY_10};

    libhei::Signature sig{ocmb0, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig);
    ServiceData sd{sig, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig)->resolve(sd);

    nlohmann::json j{};
    std::string s{};

    // Callout list
    j = sd.getCalloutList();
    s = R"([
    {
        "Deconfigured": false,
        "EntityPath": [],
        "GuardType": "GARD_Unrecoverable",
        "Guarded": true,
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0",
        "Priority": "H"
    },
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc0",
        "Priority": "L"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Callout Type": "Hardware Callout",
        "Guard": true,
        "Priority": "high",
        "Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": false,
        "Priority": "low",
        "Target": "/proc0"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Sub-test #4 - PLL unlock on multiple OCMBs in the same domain.
TEST(PllUnlock, TestSet4)
{
    pdbg_targets_init(nullptr);

    libhei::Chip ocmb0{
        util::pdbg::getTrgt("/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"),
        ODYSSEY_10};

    libhei::Chip ocmb1{
        util::pdbg::getTrgt("/proc0/pib/perv12/mc0/mi0/mcc0/omi1/ocmb0"),
        ODYSSEY_10};

    libhei::Signature sig0{ocmb0, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};
    libhei::Signature sig1{ocmb1, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig0);
    isoData.addSignature(sig1);
    ServiceData sd{sig0, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig0)->resolve(sd);

    nlohmann::json j{};
    std::string s{};

    // Callout list
    j = sd.getCalloutList();
    s = R"([
    {
        "Deconfigured": false,
        "EntityPath": [],
        "GuardType": "GARD_Unrecoverable",
        "Guarded": true,
        "LocationCode": "/proc0",
        "Priority": "H"
    },
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0",
        "Priority": "L"
    },
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi1/ocmb0",
        "Priority": "L"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Callout Type": "Hardware Callout",
        "Guard": true,
        "Priority": "high",
        "Target": "/proc0"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": false,
        "Priority": "low",
        "Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": false,
        "Priority": "low",
        "Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi1/ocmb0"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Sub-test #5 - PLL unlock on multiple OCMBs in different domains.
TEST(PllUnlock, TestSet5)
{
    pdbg_targets_init(nullptr);

    libhei::Chip ocmb0{
        util::pdbg::getTrgt("/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"),
        ODYSSEY_10};

    libhei::Chip ocmb1{
        util::pdbg::getTrgt("/proc1/pib/perv14/mc2/mi0/mcc0/omi0/ocmb0"),
        ODYSSEY_10};

    libhei::Signature sig0{ocmb0, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};
    libhei::Signature sig1{ocmb1, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig0);
    isoData.addSignature(sig1);
    ServiceData sd{sig0, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig0)->resolve(sd);

    nlohmann::json j{};
    std::string s{};

    // Callout list
    j = sd.getCalloutList();
    s = R"([
    {
        "Deconfigured": false,
        "EntityPath": [],
        "GuardType": "GARD_Unrecoverable",
        "Guarded": true,
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0",
        "Priority": "H"
    },
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc0",
        "Priority": "L"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Callout Type": "Hardware Callout",
        "Guard": true,
        "Priority": "high",
        "Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": false,
        "Priority": "low",
        "Target": "/proc0"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Sub-test #6 - PLL unlock on mixed PROCs and OCMBs.
TEST(PllUnlock, TestSet6)
{
    pdbg_targets_init(nullptr);

    libhei::Chip proc0{util::pdbg::getTrgt("/proc0"), P10_20};
    libhei::Chip proc1{util::pdbg::getTrgt("/proc1"), P10_20};

    libhei::Chip ocmb0{
        util::pdbg::getTrgt("/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"),
        ODYSSEY_10};

    libhei::Chip ocmb1{
        util::pdbg::getTrgt("/proc1/pib/perv14/mc2/mi0/mcc0/omi0/ocmb0"),
        ODYSSEY_10};

    libhei::Signature sig0{ocmb1, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};
    libhei::Signature sig1{proc1, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};
    libhei::Signature sig2{ocmb0, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};
    libhei::Signature sig3{proc0, nodeId, 0, 0, libhei::ATTN_TYPE_RECOVERABLE};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig0);
    isoData.addSignature(sig1);
    isoData.addSignature(sig2);
    isoData.addSignature(sig3);

    RasDataParser rasData{};
    libhei::Signature rootCause;
    bool attnFound = filterRootCause(AnalysisType::SYSTEM_CHECKSTOP, isoData,
                                     rootCause, rasData);
    EXPECT_TRUE(attnFound);
    EXPECT_EQ(sig1.toUint32(), rootCause.toUint32());
}
