#include <analyzer/plugins/plugin.hpp>
#include <analyzer/resolution.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

namespace analyzer
{

//------------------------------------------------------------------------------

// Helper function to get the root cause chip target from the service data.
TARGETING::TargetPtr __getRootCauseChipTarget(const ServiceData& i_sd)
{
    auto target = util::pdbg::getTrgt(i_sd.getRootCause().getChip());
    assert(nullptr != target); // This would be a really bad bug.
    return target;
}

//------------------------------------------------------------------------------

// Helper function to get a unit target from the given unit type and position.
// A unit type of 0 indicates the chip target should be returned.
TARGETING::TargetPtr __getUnitTarget(TARGETING::TargetPtr i_chipTarget,
                                     TARGETING::TYPE i_unitType,
                                     uint8_t i_unitPos)
{
    assert(nullptr != i_chipTarget);

    auto target = i_chipTarget; // default, if i_unitType is TYPE_NA

    if (TARGETING::TYPE_NA != i_unitType)
    {
        target = util::pdbg::getChipUnit(i_chipTarget, i_unitType, i_unitPos);
        if (nullptr == target)
        {
            // Likely a bug in the RAS data files.
            throw std::logic_error(
                "Unable to find target for type " + std::to_string(i_unitType) +
                " position " + std::to_string(i_unitPos));
        }
    }

    return target;
}

//------------------------------------------------------------------------------

void HardwareCalloutResolution::resolve(ServiceData& io_sd) const
{
    // Get the target for the hardware callout.
    auto target = __getUnitTarget(__getRootCauseChipTarget(io_sd), iv_unitType,
                                  iv_unitPos);

    // Add the callout and the FFDC to the service data.
    io_sd.calloutTarget(target, iv_priority, iv_guard);
}

//------------------------------------------------------------------------------

void ConnectedCalloutResolution::resolve(ServiceData& io_sd) const
{
    // Get the chip target from the root cause signature.
    auto chipTarget = __getRootCauseChipTarget(io_sd);

    // Get the endpoint target for the receiving side of the bus.
    auto rxTarget = __getUnitTarget(chipTarget, iv_unitType, iv_unitPos);

    // Add the callout and the FFDC to the service data.
    io_sd.calloutConnected(rxTarget, iv_busType, iv_priority, iv_guard);
}

//------------------------------------------------------------------------------

void BusCalloutResolution::resolve(ServiceData& io_sd) const
{
    // Get the chip target from the root cause signature.
    auto chipTarget = __getRootCauseChipTarget(io_sd);

    // Get the endpoint target for the receiving side of the bus.
    auto rxTarget = __getUnitTarget(chipTarget, iv_unitType, iv_unitPos);

    // Add the callout and the FFDC to the service data.
    io_sd.calloutBus(rxTarget, iv_busType, iv_priority, iv_guard);
}

//------------------------------------------------------------------------------

void ClockCalloutResolution::resolve(ServiceData& io_sd) const
{
    // Add the callout and the FFDC to the service data.
    io_sd.calloutClock(iv_clockType, iv_priority, iv_guard);
}

//------------------------------------------------------------------------------

void ProcedureCalloutResolution::resolve(ServiceData& io_sd) const
{
    // Add the callout and the FFDC to the service data.
    io_sd.calloutProcedure(iv_procedure, iv_priority);
}

//------------------------------------------------------------------------------

void PartCalloutResolution::resolve(ServiceData& io_sd) const
{
    // Add the callout and the FFDC to the service data.
    io_sd.calloutPart(iv_part, iv_priority);
}

//------------------------------------------------------------------------------

void PluginResolution::resolve(ServiceData& io_sd) const
{
    // Get the plugin function and call it.

    auto chip = io_sd.getRootCause().getChip();

    auto func = PluginMap::getSingleton().get(chip.getType(), iv_name);

    func(iv_instance, chip, io_sd);
}

//------------------------------------------------------------------------------

} // namespace analyzer
