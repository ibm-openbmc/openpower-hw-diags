
#include <analyzer/plugins/plugin.hpp>
#include <hei_util.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

namespace analyzer
{

namespace P10
{

/**
 * @brief Adds all clocks/chips reporting PLL unlock attentions to the callout
 *        list.
 *
 *  Processors are always called out at medium priority and never guarded. If
 *  more than one processor is reporting a PLL unlock attention on the same
 *  clock, the clock is called out with high priority. Otherwise, the clock
 *  callout priority is medium.
 */
void pll_unlock(unsigned int i_instance, const libhei::Chip&,
                ServiceData& io_servData)
{
    auto nodeId = libhei::hash<libhei::NodeId_t>("PLL_UNLOCK");

    auto sigList = io_servData.getIsolationData().getSignatureList();

    // The PLL list is initially the same size of the signature list.
    std::vector<libhei::Signature> pllList{sigList.size()};

    // Copy all signatures that match the node ID and bit position. Note that
    // in this case the bit position is the same as the plugin instance.
    auto itr = std::copy_if(sigList.begin(), sigList.end(), pllList.begin(),
                            [&nodeId, &i_instance](const auto& s) {
        return (nodeId == s.getId() && i_instance == s.getBit());
    });

    // Shrink the size of the PLL list if necessary.
    pllList.resize(std::distance(pllList.begin(), itr));

    // The clock callout priority is dependent on the number of chips with PLL
    // unlock attentions.
    auto clockPriority = (1 < pllList.size()) ? callout::Priority::HIGH
                                              : callout::Priority::MED;

    // Callout the clock.
    auto clockCallout = (0 == i_instance) ? callout::ClockType::OSC_REF_CLOCK_0
                                          : callout::ClockType::OSC_REF_CLOCK_1;
    io_servData.calloutClock(clockCallout, clockPriority, true);

    // Callout the processors connected to this clock that are reporting PLL
    // unlock attentions. Always a medium callout and no guarding.
    for (const auto& sig : pllList)
    {
        io_servData.calloutTarget(util::pdbg::getTrgt(sig.getChip()),
                                  callout::Priority::MED, false);
    }
}

void lpc_timeout_callout(const libhei::Chip& i_chip, ServiceData& io_servData)
{
    auto target = util::pdbg::getTrgt(i_chip);
    auto path = util::pdbg::getPath(target);

    // Callout the PNOR.
    io_servData.calloutPart(callout::PartType::PNOR, callout::Priority::MED);

    // Callout the associated clock, no guard.
    auto chipPos = util::pdbg::getChipPos(target);
    if (0 == chipPos)
    {
        // Clock 0 is hardwired to proc 0.
        io_servData.calloutClock(callout::ClockType::OSC_REF_CLOCK_0,
                                 callout::Priority::MED, false);
    }
    else if (1 == chipPos)
    {
        // Clock 1 is hardwired to proc 1.
        io_servData.calloutClock(callout::ClockType::OSC_REF_CLOCK_1,
                                 callout::Priority::MED, false);
    }
    else
    {
        trace::err("LPC timeout on unexpected processor: %s", path);
    }

    // Callout the processor, no guard.
    io_servData.calloutTarget(target, callout::Priority::MED, false);
}

/**
 * @brief Queries for an LPC timeout. If present, will callout all appropriate
 *        hardware.
 */
void lpc_timeout(unsigned int, const libhei::Chip& i_chip,
                 ServiceData& io_servData)
{
    auto target = util::pdbg::getTrgt(i_chip);
    auto path = util::pdbg::getPath(target);

    if (util::pdbg::queryLpcTimeout(target))
    {
        trace::inf("LPC timeout detected on %s", path);

        lpc_timeout_callout(i_chip, io_servData);
    }
    else
    {
        trace::inf("No LPC timeout detected on %s", path);

        io_servData.calloutProcedure(callout::Procedure::NEXTLVL,
                                     callout::Priority::HIGH);
    }
}

/**
 * @brief If Hostboot detects an LPC timeout, it will manually trigger a
 *        checkstop attention. We will have to bypass checking for an LPC
 *        timeout via the HWP because it will not find the timeout. Instead,
 *        simply make the callout when Hostboot triggers the attention.
 */
void lpc_timeout_workaround(unsigned int, const libhei::Chip& i_chip,
                            ServiceData& io_servData)
{
    trace::inf("Host detected LPC timeout %s", util::pdbg::getPath(i_chip));

    lpc_timeout_callout(i_chip, io_servData);
}

/**
 * @brief Calls out all DIMMs attached to an OCMB.
 */
void callout_attached_dimms(unsigned int i_instance, const libhei::Chip& i_chip,
                            ServiceData& io_servData)
{
    // Get the OMI target for this instance
    auto procTarget = util::pdbg::getTrgt(i_chip);
    auto omiTarget = util::pdbg::getChipUnit(procTarget, util::pdbg::TYPE_OMI,
                                             i_instance);

    if (nullptr != omiTarget)
    {
        // Get the connected OCMB from the OMI
        auto ocmbTarget = util::pdbg::getConnectedTarget(
            omiTarget, callout::BusType::OMI_BUS);

        // Loop through all DIMMs connected to the OCMB
        pdbg_target* dimmTarget = nullptr;
        pdbg_for_each_target("dimm", ocmbTarget, dimmTarget)
        {
            if (nullptr != dimmTarget)
            {
                // Call out the DIMM, medium priority and guard
                io_servData.calloutTarget(dimmTarget, callout::Priority::MED,
                                          true);
            }
        }
    }
}

/**
 * @brief Performs channel timeout callouts.
 */
void channel_timeout(unsigned int i_instance, const libhei::Chip& i_chip,
                     ServiceData& io_servData)
{
    // Get the OMI target for this instance
    auto procTarget = util::pdbg::getTrgt(i_chip);
    auto omiTarget = util::pdbg::getChipUnit(procTarget, util::pdbg::TYPE_OMI,
                                             i_instance);

    if (nullptr != omiTarget)
    {
        // Callout the bus and both endpoints, low priority
        io_servData.calloutBus(omiTarget, callout::BusType::OMI_BUS,
                               callout::Priority::LOW, false);

        auto sigs = io_servData.getIsolationData().getSignatureList();

        // Check if multiple channel timeout bits (MC_DSTL_FIR[22,23]) are on.
        const auto dstlfir = libhei::hash<libhei::NodeId_t>("MC_DSTL_FIR");

        // Check for the first channel timeout
        auto itr = std::find_if(sigs.begin(), sigs.end(), [&](const auto& t) {
            return (i_chip == t.getChip() && dstlfir == t.getId() &&
                    (22 == t.getBit() || 23 == t.getBit()));
        });
        if (sigs.end() != itr)
        {
            // Check for a second channel timeout starting from after itr
            itr = std::find_if(++itr, sigs.end(), [&](const auto& t) {
                return (i_chip == t.getChip() && dstlfir == t.getId() &&
                        (22 == t.getBit() || 23 == t.getBit()));
            });
        }

        // Multiple chnl timeouts found, callout the proc side high priority
        if (sigs.end() != itr)
        {
            io_servData.calloutTarget(omiTarget, callout::Priority::HIGH, true);
        }
        // Only one chnl timeout, callout the OCMB side high priority
        else
        {
            io_servData.calloutConnected(omiTarget, callout::BusType::OMI_BUS,
                                         callout::Priority::HIGH, true);
        }
    }
    else
    {
        trace::err("channel_timeout: Failed to get OMI target %d on %s",
                   i_instance, util::pdbg::getPath(procTarget));
    }
}

} // namespace P10

PLUGIN_DEFINE_NS(P10_10, P10, pll_unlock);
PLUGIN_DEFINE_NS(P10_20, P10, pll_unlock);

PLUGIN_DEFINE_NS(P10_10, P10, lpc_timeout);
PLUGIN_DEFINE_NS(P10_20, P10, lpc_timeout);

PLUGIN_DEFINE_NS(P10_10, P10, lpc_timeout_workaround);
PLUGIN_DEFINE_NS(P10_20, P10, lpc_timeout_workaround);

PLUGIN_DEFINE_NS(P10_10, P10, callout_attached_dimms);
PLUGIN_DEFINE_NS(P10_20, P10, callout_attached_dimms);

PLUGIN_DEFINE_NS(P10_10, P10, channel_timeout);
PLUGIN_DEFINE_NS(P10_20, P10, channel_timeout);

} // namespace analyzer
