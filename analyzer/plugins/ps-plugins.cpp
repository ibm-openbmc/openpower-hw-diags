#include <analyzer/plugins/plugin.hpp>
#include <hei_util.hpp>
#include <util/pdbg.hpp>

namespace analyzer
{

namespace PS
{

void omi_degrade(unsigned int, const libhei::Chip&, ServiceData&)
{
    // TODO
}

//------------------------------------------------------------------------------

void channel_timeout(unsigned int i_instance, const libhei::Chip& i_chip,
                     ServiceData& io_servData)
{
    // Get the OMI target for this instance
    auto hubTarget = util::pdbg::getTrgt(i_chip);
    auto omiTarget =
        util::pdbg::getChipUnit(hubTarget, TARGETING::TYPE_OMI, i_instance);

    if (nullptr != omiTarget)
    {
        // Callout the bus and both endpoints, low priority
        io_servData.calloutBus(omiTarget, callout::BusType::OMI_BUS,
                               callout::Priority::LOW, false);

        auto sigs = io_servData.getIsolationData().getSignatureList();

        // Check if multiple channel timeout bits (MC_DSTL_FIR[23,24]) are on.
        const auto dstlfir = libhei::hash<libhei::NodeId_t>("MC_DSTL_FIR");

        // Check for the first channel timeout
        auto itr = std::find_if(sigs.begin(), sigs.end(), [&](const auto& t) {
            return (i_chip == t.getChip() && dstlfir == t.getId() &&
                    (23 == t.getBit() || 24 == t.getBit()));
        });
        if (sigs.end() != itr)
        {
            // Check for a second channel timeout starting from after itr
            itr = std::find_if(++itr, sigs.end(), [&](const auto& t) {
                return (i_chip == t.getChip() && dstlfir == t.getId() &&
                        (23 == t.getBit() || 24 == t.getBit()));
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
                   i_instance, util::pdbg::getPath(hubTarget).c_str());
    }
}

} // namespace PS

PLUGIN_DEFINE_NS(PS_10, PS, omi_degrade);
PLUGIN_DEFINE_NS(PS_20, PS, omi_degrade);

PLUGIN_DEFINE_NS(PS_10, PS, channel_timeout);
PLUGIN_DEFINE_NS(PS_20, PS, channel_timeout);

} // namespace analyzer
