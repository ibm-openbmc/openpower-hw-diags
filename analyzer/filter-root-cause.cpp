#include <assert.h>

#include <analyzer/analyzer_main.hpp>
#include <analyzer/ras-data/ras-data-parser.hpp>
#include <hei_main.hpp>
#include <hei_util.hpp>
#include <util/pdbg.hpp>

#include <algorithm>
#include <limits>
#include <string>

namespace analyzer
{
//------------------------------------------------------------------------------

bool __findRcsOscError(const std::vector<libhei::Signature>& i_list,
                       libhei::Signature& o_rootCause)
{
    // TODO: Consider returning all of them instead of one as root cause.
    auto itr = std::find_if(i_list.begin(), i_list.end(), [&](const auto& t) {
        return (libhei::hash<libhei::NodeId_t>("TP_LOCAL_FIR") == t.getId() &&
                (42 == t.getBit() || 43 == t.getBit()));
    });

    if (i_list.end() != itr)
    {
        o_rootCause = *itr;
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------

bool __findPllUnlock(const std::vector<libhei::Signature>& i_list,
                     libhei::Signature& o_rootCause)
{
    using namespace util::pdbg;

    // TODO: Consider returning all of them instead of one as root cause.

    auto nodeId = libhei::hash<libhei::NodeId_t>("PLL_UNLOCK");

    // First, look for any PLL unlock attentions reported by a processsor chip.
    auto itr1 = std::find_if(i_list.begin(), i_list.end(), [&](const auto& t) {
        return (nodeId == t.getId() &&
                TYPE_PROC == getTrgtType(getTrgt(t.getChip())));
    });

    if (i_list.end() != itr1)
    {
        o_rootCause = *itr1;
        return true;
    }

    // Then, look for any PLL unlock attentions reported by an OCMB chip. This
    // is specifically for Odyssey, which are the only OCMBs that would report
    // PLL unlock attentions.
    auto itr2 = std::find_if(i_list.begin(), i_list.end(), [&](const auto& t) {
        return (nodeId == t.getId() &&
                TYPE_OCMB == getTrgtType(getTrgt(t.getChip())));
    });

    if (i_list.end() != itr2)
    {
        o_rootCause = *itr2;
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------

bool __findTodError(const std::vector<libhei::Signature>& i_list,
                    libhei::Signature& o_rootCause)
{
    using namespace util::pdbg;

    auto nodeId = libhei::hash<libhei::NodeId_t>("TOD_ERROR");

    // First, look for any TOD error attentions reported by a processsor chip.
    auto itr1 = std::find_if(i_list.begin(), i_list.end(), [&](const auto& t) {
        return (nodeId == t.getId() &&
                TYPE_PROC == getTrgtType(getTrgt(t.getChip())));
    });

    if (i_list.end() != itr1)
    {
        o_rootCause = *itr1;
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------

bool __findMemoryChannelFailure(const std::vector<libhei::Signature>& i_list,
                                libhei::Signature& o_rootCause,
                                const RasDataParser& i_rasData)
{
    using namespace util::pdbg;

    using func = libhei::NodeId_t (*)(const std::string& i_str);
    func __hash = libhei::hash<libhei::NodeId_t>;

    static const auto mc_dstl_fir = __hash("MC_DSTL_FIR");
    static const auto mc_ustl_fir = __hash("MC_USTL_FIR");
    static const auto mc_omi_dl_err_rpt = __hash("MC_OMI_DL_ERR_RPT");

    // First, look for any chip checkstops from the connected OCMBs.
    for (const auto& s : i_list)
    {
        if (TYPE_OCMB != getTrgtType(getTrgt(s.getChip())))
        {
            continue; // OCMBs only
        }

        // TODO: The chip data for Explorer chips currently report chip
        //       checkstops as unit checkstops. Once the chip data has been
        //       updated, the check for unit checkstops here will need to be
        //       removed.
        if (libhei::ATTN_TYPE_CHIP_CS == s.getAttnType() ||
            libhei::ATTN_TYPE_UNIT_CS == s.getAttnType())
        {
            o_rootCause = s;
            return true;
        }
    }

    // Now, look for any channel failure attentions on the processor side of the
    // memory bus.
    for (const auto& s : i_list)
    {
        if (TYPE_PROC != getTrgtType(getTrgt(s.getChip())))
        {
            continue; // processors only
        }

        // Any unit checkstop attentions that originated from the MC_DSTL_FIR or
        // MC_USTLFIR are considered a channel failure attention.
        // TODO: The "channel failure" designation is actually configurable via
        //       other registers. We just happen to expect anything that is
        //       configured to channel failure to also be configured to unit
        //       checkstop. Eventually, we will need some mechanism to check the
        //       configuration registers for a more accurate analysis.
        if (libhei::ATTN_TYPE_UNIT_CS == s.getAttnType() &&
            (mc_dstl_fir == s.getId() || mc_ustl_fir == s.getId()) &&
            !i_rasData.isFlagSet(s,
                                 RasDataParser::RasDataFlags::ATTN_FROM_OCMB))
        {
            o_rootCause = s;
            return true;
        }
        // Any signatures from MC_OMI_DL_ERR_RPT feed into the only bits in
        // MC_OMI_DL_FIR that are hardwired to channel failure.
        else if (mc_omi_dl_err_rpt == s.getId())
        {
            o_rootCause = s;
            return true;
        }
    }

    return false; // default, nothing found
}

//------------------------------------------------------------------------------

// Will query if a signature is a potential system checkstop root cause.
// attention. Note that this function excludes memory channel failure attentions
// which are checked in __findMemoryChannelFailure().
bool __findCsRootCause(const libhei::Signature& i_signature,
                       const RasDataParser& i_rasData)
{
    // Check if the input signature has the CS_POSSIBLE or SUE_SOURCE flag set.
    if (i_rasData.isFlagSet(i_signature,
                            RasDataParser::RasDataFlags::CS_POSSIBLE) ||
        i_rasData.isFlagSet(i_signature,
                            RasDataParser::RasDataFlags::SUE_SOURCE))
    {
        return true;
    }

    return false; // default, nothing found
}

//------------------------------------------------------------------------------

bool __findCsRootCause_RE(const std::vector<libhei::Signature>& i_list,
                          libhei::Signature& o_rootCause,
                          const RasDataParser& i_rasData)
{
    for (const auto& s : i_list)
    {
        // Only looking for recoverable attentions.
        if (libhei::ATTN_TYPE_RECOVERABLE != s.getAttnType())
        {
            continue;
        }

        if (__findCsRootCause(s, i_rasData))
        {
            o_rootCause = s;
            return true;
        }
    }

    return false; // default, nothing found
}

//------------------------------------------------------------------------------

bool __findCsRootCause_UCS(const std::vector<libhei::Signature>& i_list,
                           libhei::Signature& o_rootCause,
                           const RasDataParser& i_rasData)
{
    for (const auto& s : i_list)
    {
        // Only looking for unit checkstop attentions.
        if (libhei::ATTN_TYPE_UNIT_CS != s.getAttnType())
        {
            continue;
        }

        if (__findCsRootCause(s, i_rasData))
        {
            o_rootCause = s;
            return true;
        }
    }

    return false; // default, nothing found
}

//------------------------------------------------------------------------------

bool __findOcmbAttnBits(const std::vector<libhei::Signature>& i_list,
                        libhei::Signature& o_rootCause,
                        const RasDataParser& i_rasData)
{
    using namespace util::pdbg;

    // If we have any attentions from an OCMB, assume isolation to the OCMBs
    // was successful and the ATTN_FROM_OCMB flag does not need to be checked.
    for (const auto& s : i_list)
    {
        if (TYPE_OCMB == getTrgtType(getTrgt(s.getChip())))
        {
            return false;
        }
    }

    for (const auto& s : i_list)
    {
        if (i_rasData.isFlagSet(s, RasDataParser::RasDataFlags::ATTN_FROM_OCMB))
        {
            o_rootCause = s;
            return true;
        }
    }

    return false; // default, nothing found
}

//------------------------------------------------------------------------------

bool __findNonExternalCs(const std::vector<libhei::Signature>& i_list,
                         libhei::Signature& o_rootCause)
{
    using namespace util::pdbg;

    static const auto pb_ext_fir = libhei::hash<libhei::NodeId_t>("PB_EXT_FIR");

    for (const auto& s : i_list)
    {
        const auto targetType = getTrgtType(getTrgt(s.getChip()));
        const auto id = s.getId();
        const auto attnType = s.getAttnType();

        // Find any processor with chip checkstop attention that did not
        // originate from the PB_EXT_FIR.
        if ((TYPE_PROC == targetType) &&
            (libhei::ATTN_TYPE_CHIP_CS == attnType) && (pb_ext_fir != id))
        {
            o_rootCause = s;
            return true;
        }
    }

    return false; // default, nothing found
}

//------------------------------------------------------------------------------

bool __findTiRootCause(const std::vector<libhei::Signature>& i_list,
                       libhei::Signature& o_rootCause)
{
    using namespace util::pdbg;

    using func = libhei::NodeId_t (*)(const std::string& i_str);
    func __hash = libhei::hash<libhei::NodeId_t>;

    // PROC registers
    static const auto tp_local_fir = __hash("TP_LOCAL_FIR");
    static const auto occ_fir = __hash("OCC_FIR");
    static const auto pbao_fir = __hash("PBAO_FIR");
    static const auto n0_local_fir = __hash("N0_LOCAL_FIR");
    static const auto int_cq_fir = __hash("INT_CQ_FIR");
    static const auto nx_cq_fir = __hash("NX_CQ_FIR");
    static const auto nx_dma_eng_fir = __hash("NX_DMA_ENG_FIR");
    static const auto vas_fir = __hash("VAS_FIR");
    static const auto n1_local_fir = __hash("N1_LOCAL_FIR");
    static const auto mcd_fir = __hash("MCD_FIR");
    static const auto pb_station_fir_en_1 = __hash("PB_STATION_FIR_EN_1");
    static const auto pb_station_fir_en_2 = __hash("PB_STATION_FIR_EN_2");
    static const auto pb_station_fir_en_3 = __hash("PB_STATION_FIR_EN_3");
    static const auto pb_station_fir_en_4 = __hash("PB_STATION_FIR_EN_4");
    static const auto pb_station_fir_es_1 = __hash("PB_STATION_FIR_ES_1");
    static const auto pb_station_fir_es_2 = __hash("PB_STATION_FIR_ES_2");
    static const auto pb_station_fir_es_3 = __hash("PB_STATION_FIR_ES_3");
    static const auto pb_station_fir_es_4 = __hash("PB_STATION_FIR_ES_4");
    static const auto pb_station_fir_eq = __hash("PB_STATION_FIR_EQ");
    static const auto psihb_fir = __hash("PSIHB_FIR");
    static const auto pbaf_fir = __hash("PBAF_FIR");
    static const auto lpc_fir = __hash("LPC_FIR");
    static const auto eq_core_fir = __hash("EQ_CORE_FIR");
    static const auto eq_l2_fir = __hash("EQ_L2_FIR");
    static const auto eq_l3_fir = __hash("EQ_L3_FIR");
    static const auto eq_ncu_fir = __hash("EQ_NCU_FIR");
    static const auto eq_local_fir = __hash("EQ_LOCAL_FIR");
    static const auto eq_qme_fir = __hash("EQ_QME_FIR");
    static const auto iohs_local_fir = __hash("IOHS_LOCAL_FIR");
    static const auto iohs_dlp_fir_oc = __hash("IOHS_DLP_FIR_OC");
    static const auto iohs_dlp_fir_smp = __hash("IOHS_DLP_FIR_SMP");
    static const auto mc_local_fir = __hash("MC_LOCAL_FIR");
    static const auto mc_fir = __hash("MC_FIR");
    static const auto mc_dstl_fir = __hash("MC_DSTL_FIR");
    static const auto mc_ustl_fir = __hash("MC_USTL_FIR");
    static const auto nmmu_cq_fir = __hash("NMMU_CQ_FIR");
    static const auto nmmu_fir = __hash("NMMU_FIR");
    static const auto mc_omi_dl = __hash("MC_OMI_DL");
    static const auto pau_local_fir = __hash("PAU_LOCAL_FIR");
    static const auto pau_ptl_fir = __hash("PAU_PTL_FIR");
    static const auto pau_phy_fir = __hash("PAU_PHY_FIR");
    static const auto pau_fir_0 = __hash("PAU_FIR_0");
    static const auto pau_fir_2 = __hash("PAU_FIR_2");
    static const auto pci_local_fir = __hash("PCI_LOCAL_FIR");
    static const auto pci_iop_fir = __hash("PCI_IOP_FIR");
    static const auto pci_nest_fir = __hash("PCI_NEST_FIR");

    // OCMB registers
    static const auto ocmb_lfir = __hash("OCMB_LFIR");
    static const auto mmiofir = __hash("MMIOFIR");
    static const auto srqfir = __hash("SRQFIR");
    static const auto rdffir = __hash("RDFFIR");
    static const auto tlxfir = __hash("TLXFIR");
    static const auto omi_dl = __hash("OMI_DL");

    for (const auto& signature : i_list)
    {
        const auto targetType = getTrgtType(getTrgt(signature.getChip()));
        const auto attnType = signature.getAttnType();
        const auto id = signature.getId();
        const auto bit = signature.getBit();

        // Only looking for recoverable or unit checkstop attentions.
        if (libhei::ATTN_TYPE_RECOVERABLE != attnType &&
            libhei::ATTN_TYPE_UNIT_CS != attnType)
        {
            continue;
        }

        // Ignore attentions that should not be blamed as root cause of a TI.
        // This would include informational only FIRs or correctable errors.
        if (TYPE_PROC == targetType)
        {
            if (tp_local_fir == id &&
                (0 == bit || 1 == bit || 2 == bit || 3 == bit || 4 == bit ||
                 5 == bit || 7 == bit || 8 == bit || 9 == bit || 10 == bit ||
                 11 == bit || 20 == bit || 22 == bit || 23 == bit ||
                 24 == bit || 38 == bit || 40 == bit || 41 == bit ||
                 46 == bit || 47 == bit || 48 == bit || 55 == bit ||
                 56 == bit || 57 == bit || 58 == bit || 59 == bit))
            {
                continue;
            }

            if (occ_fir == id &&
                (9 == bit || 10 == bit || 15 == bit || 20 == bit || 21 == bit ||
                 22 == bit || 23 == bit || 32 == bit || 33 == bit ||
                 34 == bit || 36 == bit || 42 == bit || 43 == bit ||
                 46 == bit || 47 == bit || 48 == bit || 51 == bit ||
                 52 == bit || 53 == bit || 54 == bit || 57 == bit))
            {
                continue;
            }

            if (pbao_fir == id &&
                (0 == bit || 1 == bit || 2 == bit || 8 == bit || 11 == bit ||
                 13 == bit || 15 == bit || 16 == bit || 17 == bit))
            {
                continue;
            }

            if ((n0_local_fir == id || n1_local_fir == id ||
                 iohs_local_fir == id || mc_local_fir == id ||
                 pau_local_fir == id || pci_local_fir == id) &&
                (0 == bit || 1 == bit || 2 == bit || 3 == bit || 4 == bit ||
                 5 == bit || 6 == bit || 7 == bit || 8 == bit || 9 == bit ||
                 10 == bit || 11 == bit || 20 == bit || 21 == bit))
            {
                continue;
            }

            if (int_cq_fir == id &&
                (0 == bit || 3 == bit || 5 == bit || 7 == bit || 36 == bit ||
                 47 == bit || 48 == bit || 49 == bit || 50 == bit ||
                 58 == bit || 59 == bit || 60 == bit))
            {
                continue;
            }

            if (nx_cq_fir == id &&
                (1 == bit || 4 == bit || 18 == bit || 32 == bit || 33 == bit))
            {
                continue;
            }

            if (nx_dma_eng_fir == id &&
                (4 == bit || 6 == bit || 9 == bit || 10 == bit || 11 == bit ||
                 34 == bit || 35 == bit || 36 == bit || 37 == bit || 39 == bit))
            {
                continue;
            }

            if (vas_fir == id &&
                (8 == bit || 9 == bit || 11 == bit || 12 == bit || 13 == bit))
            {
                continue;
            }

            if (mcd_fir == id && (0 == bit))
            {
                continue;
            }

            if ((pb_station_fir_en_1 == id || pb_station_fir_en_2 == id ||
                 pb_station_fir_en_3 == id || pb_station_fir_en_4 == id ||
                 pb_station_fir_es_1 == id || pb_station_fir_es_2 == id ||
                 pb_station_fir_es_3 == id || pb_station_fir_es_4 == id ||
                 pb_station_fir_eq == id) &&
                (9 == bit))
            {
                continue;
            }

            if (psihb_fir == id && (0 == bit || 23 == bit))
            {
                continue;
            }

            if (pbaf_fir == id &&
                (0 == bit || 1 == bit || 3 == bit || 4 == bit || 5 == bit ||
                 6 == bit || 7 == bit || 8 == bit || 9 == bit || 10 == bit ||
                 11 == bit || 19 == bit || 20 == bit || 21 == bit ||
                 28 == bit || 29 == bit || 30 == bit || 31 == bit ||
                 32 == bit || 33 == bit || 34 == bit || 35 == bit || 36 == bit))
            {
                continue;
            }

            if (lpc_fir == id && (5 == bit))
            {
                continue;
            }

            if (eq_core_fir == id &&
                (0 == bit || 2 == bit || 4 == bit || 7 == bit || 9 == bit ||
                 11 == bit || 13 == bit || 18 == bit || 21 == bit ||
                 24 == bit || 29 == bit || 31 == bit || 37 == bit ||
                 43 == bit || 56 == bit || 57 == bit))
            {
                continue;
            }

            if (eq_l2_fir == id &&
                (0 == bit || 6 == bit || 11 == bit || 19 == bit || 36 == bit))
            {
                continue;
            }

            if (eq_l3_fir == id &&
                (3 == bit || 4 == bit || 7 == bit || 10 == bit || 13 == bit))
            {
                continue;
            }

            if (eq_ncu_fir == id && (9 == bit))
            {
                continue;
            }

            if (eq_local_fir == id &&
                (0 == bit || 1 == bit || 2 == bit || 3 == bit || 5 == bit ||
                 6 == bit || 7 == bit || 8 == bit || 9 == bit || 10 == bit ||
                 11 == bit || 12 == bit || 13 == bit || 14 == bit ||
                 15 == bit || 16 == bit || 20 == bit || 21 == bit ||
                 22 == bit || 23 == bit || 24 == bit || 25 == bit ||
                 26 == bit || 27 == bit || 28 == bit || 29 == bit ||
                 30 == bit || 31 == bit || 32 == bit || 33 == bit ||
                 34 == bit || 35 == bit || 36 == bit || 37 == bit ||
                 38 == bit || 39 == bit))
            {
                continue;
            }

            if (eq_qme_fir == id && (7 == bit || 25 == bit))
            {
                continue;
            }

            if (iohs_dlp_fir_oc == id &&
                (6 == bit || 7 == bit || 8 == bit || 9 == bit || 10 == bit ||
                 48 == bit || 49 == bit || 52 == bit || 53 == bit))
            {
                continue;
            }

            if (iohs_dlp_fir_smp == id &&
                (6 == bit || 7 == bit || 14 == bit || 15 == bit || 16 == bit ||
                 17 == bit || 38 == bit || 39 == bit || 44 == bit ||
                 45 == bit || 50 == bit || 51 == bit))
            {
                continue;
            }

            if (mc_fir == id &&
                (5 == bit || 8 == bit || 15 == bit || 16 == bit))
            {
                continue;
            }

            if (mc_dstl_fir == id &&
                (0 == bit || 1 == bit || 2 == bit || 3 == bit || 4 == bit ||
                 5 == bit || 6 == bit || 7 == bit || 14 == bit || 15 == bit))
            {
                continue;
            }

            if (mc_ustl_fir == id &&
                (6 == bit || 20 == bit || 33 == bit || 34 == bit))
            {
                continue;
            }

            if (nmmu_cq_fir == id && (8 == bit || 11 == bit || 14 == bit))
            {
                continue;
            }

            if (nmmu_fir == id &&
                (0 == bit || 3 == bit || 8 == bit || 9 == bit || 10 == bit ||
                 11 == bit || 12 == bit || 13 == bit || 14 == bit ||
                 15 == bit || 30 == bit || 31 == bit || 41 == bit))
            {
                continue;
            }

            if (mc_omi_dl == id && (2 == bit || 3 == bit || 6 == bit ||
                                    7 == bit || 9 == bit || 10 == bit))
            {
                continue;
            }

            if (pau_ptl_fir == id && (5 == bit || 9 == bit))
            {
                continue;
            }

            if (pau_phy_fir == id &&
                (2 == bit || 3 == bit || 6 == bit || 7 == bit || 15 == bit))
            {
                continue;
            }

            if (pau_fir_0 == id && (13 == bit || 30 == bit || 41 == bit))
            {
                continue;
            }

            if (pau_fir_2 == id && (19 == bit || 46 == bit || 49 == bit))
            {
                continue;
            }

            if (pci_iop_fir == id &&
                (0 == bit || 2 == bit || 4 == bit || 6 == bit || 7 == bit ||
                 8 == bit || 10 == bit))
            {
                continue;
            }

            if (pci_nest_fir == id && (2 == bit || 5 == bit))
            {
                continue;
            }
        }
        else if (TYPE_OCMB == targetType)
        {
            if (ocmb_lfir == id &&
                (0 == bit || 1 == bit || 2 == bit || 8 == bit || 23 == bit ||
                 37 == bit || 63 == bit))
            {
                continue;
            }

            if (mmiofir == id && (2 == bit))
            {
                continue;
            }

            if (srqfir == id &&
                (2 == bit || 4 == bit || 14 == bit || 15 == bit || 23 == bit ||
                 25 == bit || 28 == bit))
            {
                continue;
            }

            if (rdffir == id &&
                (0 == bit || 1 == bit || 2 == bit || 3 == bit || 4 == bit ||
                 5 == bit || 6 == bit || 7 == bit || 8 == bit || 9 == bit ||
                 18 == bit || 38 == bit || 40 == bit || 41 == bit ||
                 45 == bit || 46 == bit))
            {
                continue;
            }

            if (tlxfir == id && (0 == bit || 9 == bit || 26 == bit))
            {
                continue;
            }

            if (omi_dl == id && (2 == bit || 3 == bit || 6 == bit || 7 == bit ||
                                 9 == bit || 10 == bit))
            {
                continue;
            }
        }

        // At this point, the attention has not been explicitly ignored. So
        // return this signature and exit.
        o_rootCause = signature;
        return true;
    }

    return false; // default, nothing found
}

//------------------------------------------------------------------------------

bool findRootCause(AnalysisType i_type, const libhei::IsolationData& i_isoData,
                   libhei::Signature& o_rootCause,
                   const RasDataParser& i_rasData)
{
    // We'll need to make a copy of the list so that the original list is
    // maintained for the PEL.
    std::vector<libhei::Signature> list{i_isoData.getSignatureList()};

    // START WORKAROUND
    // TODO: Filtering should be data driven. Until that support is available,
    //       use the following isolation rules.

    // Ensure the list is not empty before continuing.
    if (list.empty())
    {
        return false; // nothing more to do
    }

    // First, look for any RCS OSC errors. This must always be first because
    // they can cause downstream PLL unlock attentions.
    if (__findRcsOscError(list, o_rootCause))
    {
        return true;
    }

    // Second, look for any PLL unlock attentions. This must always be second
    // because PLL unlock attentions can cause any number of downstream
    // attentions, including a system checkstop.
    if (__findPllUnlock(list, o_rootCause))
    {
        return true;
    }

    // Third, look for any TOD error attentions. These are checkstop attentions
    // but are clock related and therefore should be prioritized over other
    // attentions.
    if (__findTodError(list, o_rootCause))
    {
        return true;
    }

    // Regardless of the analysis type, always look for anything that could be
    // blamed as the root cause of a system checkstop.

    // Memory channel failure attentions will produce SUEs and likely cause
    // downstream attentions, including a system checkstop.
    if (__findMemoryChannelFailure(list, o_rootCause, i_rasData))
    {
        return true;
    }

    // Look for any recoverable attentions that have been identified as a
    // potential root cause of a system checkstop attention. These would include
    // any attention that would generate an SUE. Note that is it possible for
    // recoverables to generate unit checkstop attentions so we must check them
    // first.
    if (__findCsRootCause_RE(list, o_rootCause, i_rasData))
    {
        return true;
    }

    // Look for any unit checkstop attentions (other than memory channel
    // failures) that have been identified as a potential root cause of a
    // system checkstop attention. These would include any attention that would
    // generate an SUE.
    if (__findCsRootCause_UCS(list, o_rootCause, i_rasData))
    {
        return true;
    }

    // If no other viable root cause has been found, check for any signatures
    // with the ATTN_FROM_OCMB flag in case there was an attention from an
    // inaccessible OCMB.
    if (__findOcmbAttnBits(list, o_rootCause, i_rasData))
    {
        return true;
    }

    // Look for any system checkstop attentions that originated from within the
    // chip that reported the attention. In other words, no external checkstop
    // attentions.
    if (__findNonExternalCs(list, o_rootCause))
    {
        return true;
    }

    if (AnalysisType::SYSTEM_CHECKSTOP != i_type)
    {
        // No system checkstop root cause attentions were found. Next, look for
        // any recoverable or unit checkstop attentions that could be associated
        // with a TI.
        if (__findTiRootCause(list, o_rootCause))
        {
            return true;
        }

        if (AnalysisType::TERMINATE_IMMEDIATE != i_type)
        {
            // No attentions associated with a system checkstop or TI were
            // found. Simply, return the first entry in the list.
            o_rootCause = list.front();
            return true;
        }
    }

    // END WORKAROUND

    return false; // default, no active attentions found.
}

//------------------------------------------------------------------------------

bool __findIueTh(const std::vector<libhei::Signature>& i_list,
                 libhei::Signature& o_rootCause)
{
    auto itr = std::find_if(i_list.begin(), i_list.end(), [&](const auto& t) {
        return (libhei::hash<libhei::NodeId_t>("RDFFIR") == t.getId() &&
                (17 == t.getBit() || 37 == t.getBit())) ||
               (libhei::hash<libhei::NodeId_t>("RDF_FIR") == t.getId() &&
                (18 == t.getBit() || 38 == t.getBit()));
    });

    if (i_list.end() != itr)
    {
        o_rootCause = *itr;
        return true;
    }

    return false;
}

//------------------------------------------------------------------------------

void rootCauseSpecialCases(const libhei::IsolationData& i_isoData,
                           libhei::Signature& o_rootCause,
                           const RasDataParser& i_rasData)
{
    using func = libhei::NodeId_t (*)(const std::string& i_str);
    func __hash = libhei::hash<libhei::NodeId_t>;

    // Check for any special cases that exist for specific FIR bits.

    // If the channel fail was specifically a firmware initiated channel fail
    // (SRQFIR[25] for Explorer OCMBs, SRQ_FIR[46] for Odyssey OCMBs) check for
    // any IUE bits that are on that would have caused the channel fail
    // (RDFFIR[17,37] for Explorer OCMBs, RDF_FIR_0[18,38] or RDF_FIR_1[18,38]
    // for Odyssey OCMBs).

    // Explorer SRQFIR
    static const auto srqfir = __hash("SRQFIR");
    // Odyssey SRQ_FIR
    static const auto srq_fir = __hash("SRQ_FIR");

    std::vector<libhei::Signature> list{i_isoData.getSignatureList()};

    if (((srqfir == o_rootCause.getId() && 25 == o_rootCause.getBit()) ||
         (srq_fir == o_rootCause.getId() && 46 == o_rootCause.getBit())) &&
        __findIueTh(list, o_rootCause))
    {
        // If __findIueTh returned true, o_rootCause was updated, return.
        return;
    }

    // Check if the root cause found was a potential side effect of an
    // ODP data corruption error. If it was, check if any other signature
    // in the signature list was a potential root cause.
    auto OdpSide = RasDataParser::RasDataFlags::ODP_DATA_CORRUPT_SIDE_EFFECT;
    auto OdpRoot = RasDataParser::RasDataFlags::ODP_DATA_CORRUPT_ROOT_CAUSE;
    if (i_rasData.isFlagSet(o_rootCause, OdpSide))
    {
        for (const auto& s : list)
        {
            if (i_rasData.isFlagSet(s, OdpRoot))
            {
                // ODP data corruption root cause found, return.
                o_rootCause = s;
                return;
            }
        }
    }

    // Odyssey RDF_FIR
    static const auto rdf_fir = __hash("RDF_FIR");

    // RDF_FIR[41] can be the root cause of RDF_FIR[16], so if bit 16 is on,
    // check if bit 41 is also on.
    if (rdf_fir == o_rootCause.getId() && 16 == o_rootCause.getBit())
    {
        // Look for RDF_FIR[41]
        auto itr = std::find_if(list.begin(), list.end(), [&](const auto& t) {
            return (rdf_fir == t.getId() && 41 == t.getBit());
        });
        if (list.end() != itr)
        {
            o_rootCause = *itr;
        }
    }
}

//------------------------------------------------------------------------------

bool filterRootCause(AnalysisType i_type,
                     const libhei::IsolationData& i_isoData,
                     libhei::Signature& o_rootCause,
                     const RasDataParser& i_rasData)
{
    // Find the initial root cause attention based on common rules for FIR
    // isolation.
    bool rc = findRootCause(i_type, i_isoData, o_rootCause, i_rasData);

    // If some root cause was found, handle any special cases for specific FIR
    // bits that require additional logic to determine the root cause.
    if (true == rc)
    {
        rootCauseSpecialCases(i_isoData, o_rootCause, i_rasData);
    }

    return rc;
}

//------------------------------------------------------------------------------

} // namespace analyzer
