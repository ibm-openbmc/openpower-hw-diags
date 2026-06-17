#include <assert.h>

#include <analyzer/analyzer_main.hpp>
#include <analyzer/plugins/plugin.hpp>
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

bool __lookForBits(const std::vector<libhei::Signature>& i_sigList,
                   libhei::Signature& o_rootCause,
                   std::vector<libhei::ChipType_t> i_chipTypes,
                   const char* i_fir, std::vector<uint8_t> i_bitList)
{
    using func = libhei::NodeId_t (*)(const std::string& i_str);
    func __hash = libhei::hash<libhei::NodeId_t>;

    auto itr =
        std::find_if(i_sigList.begin(), i_sigList.end(), [&](const auto& sig) {
            for (const auto& type : i_chipTypes)
            {
                if (type != sig.getChip().getType())
                {
                    continue;
                }
                for (const auto& bit : i_bitList)
                {
                    if (__hash(i_fir) == sig.getId() && bit == sig.getBit())
                    {
                        return true;
                    }
                    else
                    {
                        continue;
                    }
                }
            }
            return false;
        });

    if (i_sigList.end() != itr)
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
            (s.getChip().getType() == analyzer::P10_10 ||
             s.getChip().getType() == analyzer::P10_20) &&
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
                       libhei::Signature& o_rootCause,
                       const RasDataParser& i_rasData)
{
    using namespace util::pdbg;
    using rdf = RasDataParser::RasDataFlags;

    for (const auto& signature : i_list)
    {
        const auto attnType = signature.getAttnType();

        // Only looking for recoverable or unit checkstop attentions.
        if (libhei::ATTN_TYPE_RECOVERABLE != attnType &&
            libhei::ATTN_TYPE_UNIT_CS != attnType)
        {
            continue;
        }

        // Skip any signature with the 'recovered_error', 'informational_only',
        // or 'attn_from_ocmb' flags.
        if (i_rasData.isFlagSet(signature, rdf::RECOVERED_ERROR) ||
            i_rasData.isFlagSet(signature, rdf::INFORMATIONAL_ONLY) ||
            i_rasData.isFlagSet(signature, rdf::MNFG_INFORMATIONAL_ONLY) ||
            i_rasData.isFlagSet(signature, rdf::ATTN_FROM_OCMB))
        {
            continue;
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
    if (__lookForBits(list, o_rootCause, {analyzer::P10_10, analyzer::P10_20},
                      "TP_LOCAL_FIR", {42, 43}))
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
        if (__findTiRootCause(list, o_rootCause, i_rasData))
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
