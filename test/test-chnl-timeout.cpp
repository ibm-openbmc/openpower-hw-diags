#include <stdio.h>

#include <analyzer/plugins/plugin.hpp>
#include <analyzer/ras-data/ras-data-parser.hpp>
#include <hei_util.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include "gtest/gtest.h"

using namespace analyzer;

static const auto dstlfirId = static_cast<libhei::NodeId_t>(
    libhei::hash<libhei::NodeId_t>("MC_DSTL_FIR"));

// Test multiple channel timeouts on at the same time
TEST(ChnlTimeout, MultipleTimeouts)
{
    pdbg_targets_init(nullptr);

    libhei::Chip proc0{util::pdbg::getTrgt("/proc0"), P10_20};

    libhei::Signature sig1{proc0, dstlfirId, 0, 22, libhei::ATTN_TYPE_UNIT_CS};
    libhei::Signature sig2{proc0, dstlfirId, 0, 23, libhei::ATTN_TYPE_UNIT_CS};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig1);
    isoData.addSignature(sig2);
    ServiceData sd{sig1, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig1)->resolve(sd);

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
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
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
        "LocationCode": "P0",
        "Priority": "L"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Bus Type": "OMI_BUS",
        "Callout Type": "Bus Callout",
        "Guard": false,
        "Priority": "low",
        "RX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "TX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": true,
        "Priority": "high",
        "Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Test multiple channel timeouts on different MC_DSTL_FIR instances
TEST(ChnlTimeout, DifferentInst)
{
    pdbg_targets_init(nullptr);

    libhei::Chip proc0{util::pdbg::getTrgt("/proc0"), P10_20};

    libhei::Signature sig1{proc0, dstlfirId, 0, 22, libhei::ATTN_TYPE_UNIT_CS};
    libhei::Signature sig2{proc0, dstlfirId, 1, 22, libhei::ATTN_TYPE_UNIT_CS};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig1);
    isoData.addSignature(sig2);
    ServiceData sd{sig1, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig1)->resolve(sd);

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
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
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
        "LocationCode": "P0",
        "Priority": "L"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Bus Type": "OMI_BUS",
        "Callout Type": "Bus Callout",
        "Guard": false,
        "Priority": "low",
        "RX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "TX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    },
    {
        "Callout Type": "Hardware Callout",
        "Guard": true,
        "Priority": "high",
        "Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Test multiple channel timeouts on different processors
TEST(ChnlTimeout, DifferentProc)
{
    pdbg_targets_init(nullptr);

    libhei::Chip proc0{util::pdbg::getTrgt("/proc0"), P10_20};
    libhei::Chip proc1{util::pdbg::getTrgt("/proc1"), P10_20};

    libhei::Signature sig1{proc0, dstlfirId, 0, 22, libhei::ATTN_TYPE_UNIT_CS};
    libhei::Signature sig2{proc1, dstlfirId, 0, 22, libhei::ATTN_TYPE_UNIT_CS};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig1);
    isoData.addSignature(sig2);
    ServiceData sd{sig1, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig1)->resolve(sd);

    nlohmann::json j{};
    std::string s{};

    // Callout list
    j = sd.getCalloutList();
    s = R"([
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "Priority": "L"
    },
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
        "LocationCode": "P0",
        "Priority": "L"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Bus Type": "OMI_BUS",
        "Callout Type": "Bus Callout",
        "Guard": false,
        "Priority": "low",
        "RX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "TX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    },
    {
        "Bus Type": "OMI_BUS",
        "Callout Type": "Connected Callout",
        "Guard": true,
        "Priority": "high",
        "RX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "TX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}

// Test a single channel timeout
TEST(ChnlTimeout, SingleTimeout)
{
    pdbg_targets_init(nullptr);

    libhei::Chip proc0{util::pdbg::getTrgt("/proc0"), P10_20};

    libhei::Signature sig1{proc0, dstlfirId, 0, 22, libhei::ATTN_TYPE_UNIT_CS};

    libhei::IsolationData isoData{};
    isoData.addSignature(sig1);
    ServiceData sd{sig1, AnalysisType::SYSTEM_CHECKSTOP, isoData};

    RasDataParser rasData{};
    rasData.getResolution(sig1)->resolve(sd);

    nlohmann::json j{};
    std::string s{};

    // Callout list
    j = sd.getCalloutList();
    s = R"([
    {
        "Deconfigured": false,
        "Guarded": false,
        "LocationCode": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "Priority": "L"
    },
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
        "LocationCode": "P0",
        "Priority": "L"
    }
])";
    EXPECT_EQ(s, j.dump(4));

    // Callout FFDC
    j = sd.getCalloutFFDC();
    s = R"([
    {
        "Bus Type": "OMI_BUS",
        "Callout Type": "Bus Callout",
        "Guard": false,
        "Priority": "low",
        "RX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "TX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    },
    {
        "Bus Type": "OMI_BUS",
        "Callout Type": "Connected Callout",
        "Guard": true,
        "Priority": "high",
        "RX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0",
        "TX Target": "/proc0/pib/perv12/mc0/mi0/mcc0/omi0/ocmb0"
    }
])";
    EXPECT_EQ(s, j.dump(4));
}
