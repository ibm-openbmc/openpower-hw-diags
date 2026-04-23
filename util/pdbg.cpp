//------------------------------------------------------------------------------
// IMPORTANT:
// This file will be built in CI test and should work out-of-the-box in CI test
// with use of the fake device tree. Any functions that require addition support
// to simulate in CI test should be put in `pdbg_no_sim.cpp`.
//------------------------------------------------------------------------------

#include <assert.h>
#include <config.h>

#include <hei_main.hpp>
#include <nlohmann/json.hpp>
#include <util/dbus.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include <filesystem>
#include <fstream>
#include <string>

using namespace analyzer;

namespace fs = std::filesystem;

namespace util
{

namespace pdbg
{

//------------------------------------------------------------------------------

TARGETING::TargetPtr getTrgt(const libhei::Chip& i_chip)
{
    return (TARGETING::TargetPtr)i_chip.getChip();
}

//------------------------------------------------------------------------------

const std::string getPath(TARGETING::TargetPtr i_target)
{
    return TARGETING::utils::getPhysicalPath(i_target);
}

const std::string getPath(const libhei::Chip& i_chip)
{
    return getPath(getTrgt(i_chip));
}

//------------------------------------------------------------------------------

uint32_t getChipPos(TARGETING::TargetPtr i_target)
{
    return i_target->getAttr<TARGETING::ATTR_FAPI_POS>();
}

uint32_t getChipPos(const libhei::Chip& i_chip)
{
    return getChipPos(getTrgt(i_chip));
}

//------------------------------------------------------------------------------

uint8_t getUnitPos(TARGETING::TargetPtr i_target)
{
    return TARGETING::utils::getChipUnitPos(i_target);
}

//------------------------------------------------------------------------------

uint8_t getTrgtType(TARGETING::TargetPtr i_target)
{
    return i_target->getAttr<TARGETING::ATTR_TYPE>();
}

uint8_t getTrgtType(const libhei::Chip& i_chip)
{
    return getTrgtType(getTrgt(i_chip));
}

//------------------------------------------------------------------------------

TARGETING::TargetPtr getParentChip(TARGETING::TargetPtr i_unitTarget)
{
    assert(nullptr != i_unitTarget);

    // Check if the given target is already a chip.
    auto targetType = getTrgtType(i_unitTarget);
    if (TARGETING::TYPE_HUB_CHIP == targetType ||
        TARGETING::TYPE_OCMB_CHIP == targetType ||
        TARGETING::TYPE_COMPUTE_CHIP == targetType)
    {
        return i_unitTarget; // simply return the given target
    }

    // Check if this unit is on an OCMB.
    TARGETING::TargetPtr parentChip = TARGETING::utils::getParentTarget(
        i_unitTarget, TARGETING::TYPE_OCMB_CHIP);

    // If not on the OCMB, check if this unit is on a HUB.
    if (nullptr == parentChip)
    {
        parentChip = TARGETING::utils::getParentTarget(
            i_unitTarget, TARGETING::TYPE_HUB_CHIP);
    }

    // If not on an OCMB or HUB, check if this unit is on a COMPUTE chip.
    if (nullptr == parentChip)
    {
        parentChip = TARGETING::utils::getParentTarget(
            i_unitTarget, TARGETING::TYPE_COMPUTE_CHIP);
    }

    // There should always be a parent chip. Throw an error if not found.
    if (nullptr == parentChip)
    {
        throw std::logic_error("No parent chip found: i_unitTarget=" +
                               std::string{getPath(i_unitTarget)});
    }

    return parentChip;
}

//------------------------------------------------------------------------------

TARGETING::TargetPtr getParentHub(TARGETING::TargetPtr i_target)
{
    assert(nullptr != i_target);

    // Check if the given target is already a processor chip.
    if (TARGETING::TYPE_HUB_CHIP == getTrgtType(i_target))
    {
        return i_target; // simply return the given target
    }

    // Get the parent processor chip.
    TARGETING::TargetPtr parentChip =
        TARGETING::utils::getParentTarget(i_target, TARGETING::TYPE_HUB_CHIP);

    // There should always be a parent chip. Throw an error if not found.
    if (nullptr == parentChip)
    {
        throw std::logic_error(
            "No parent chip found: i_target=" + std::string{getPath(i_target)});
    }

    return parentChip;
}

//------------------------------------------------------------------------------

TARGETING::TargetPtr getChipUnit(TARGETING::TargetPtr i_parentChip,
                                 TARGETING::TYPE i_unitType, uint8_t i_unitPos)
{
    assert(nullptr != i_parentChip);

    // Iterate all children of the parent and match the unit position.
    TARGETING::TargetPtr unitTarget = nullptr;
    for (const auto& u :
         TARGETING::utils::getChildTargets(i_parentChip, i_unitType))
    {
        if (nullptr != u && i_unitPos == getUnitPos(u))
        {
            unitTarget = u;
            break; // found it
        }
    }

    // Print a warning if the target unit is not found, but don't throw an
    // error.  Instead let the calling code deal with it.
    if (nullptr == unitTarget)
    {
        trace::err("No unit target found: i_parentChip=%s i_unitType=0x%02x "
                   "i_unitPos=%u",
                   getPath(i_parentChip), i_unitType, i_unitPos);
    }

    return unitTarget;
}

//------------------------------------------------------------------------------

TARGETING::TargetPtr getTargetAcrossBus(TARGETING::TargetPtr i_rxTarget)
{
    assert(nullptr != i_rxTarget);

    TARGETING::TargetPtr o_peerTarget = nullptr;

    TARGETING::EntityPath peerPath;
    if (i_rxTarget->tryGetAttr<TARGETING::ATTR_PEER_PATH>(peerPath))
    {
        auto& targetService = TARGETING::TargetService::instance();
        o_peerTarget = targetService.toTarget(peerPath);
    }

    return o_peerTarget;
}

//------------------------------------------------------------------------------

TARGETING::TargetPtr getConnectedTarget(TARGETING::TargetPtr i_rxTarget,
                                        const callout::BusType& i_busType)
{
    assert(nullptr != i_rxTarget);

    TARGETING::TargetPtr txTarget = nullptr;

    auto rxType = util::pdbg::getTrgtType(i_rxTarget);
    std::string rxPath = util::pdbg::getPath(i_rxTarget);

    if (callout::BusType::SMP_BUS == i_busType &&
        TARGETING::TYPE_SMPGROUP == rxType)
    {
        txTarget = getTargetAcrossBus(i_rxTarget);
    }
    else if (callout::BusType::SMP_BUS == i_busType &&
             TARGETING::TYPE_IOHS == rxType)
    {
        txTarget = getTargetAcrossBus(i_rxTarget);
    }
    else if (callout::BusType::OMI_BUS == i_busType &&
             TARGETING::TYPE_OMI == rxType)
    {
        TARGETING::TargetPtrList childList = TARGETING::utils::getChildTargets(
            i_rxTarget, TARGETING::TYPE_OCMB_CHIP);

        // We know there should only be one OCMB per OMI.
        if (1 != childList.size())
        {
            throw std::logic_error("Invalid child list size for " + rxPath);
        }

        // Get the connected target.
        txTarget = childList.front();
    }
    else if (callout::BusType::OMI_BUS == i_busType &&
             TARGETING::TYPE_OCMB_CHIP == rxType)
    {
        txTarget =
            TARGETING::utils::getParentTarget(i_rxTarget, TARGETING::TYPE_OMI);
        if (nullptr == txTarget)
        {
            throw std::logic_error("No parent OMI found for " + rxPath);
        }
    }
    else
    {
        // This would be a code bug.
        throw std::logic_error("Unsupported config: i_rxTarget=" + rxPath +
                               " i_busType=" + i_busType.getString());
    }

    assert(nullptr != txTarget); // just in case we missed something above

    return txTarget;
}

//------------------------------------------------------------------------------

// IMPORTANT:
// The ATTR_CHIP_ID attribute will be synced from Hostboot to the BMC at
// some point during the IPL. It is possible that this information is needed
// before the sync occurs, in which case the value will return 0.
uint32_t __getChipId(TARGETING::TargetPtr i_target)
{
    return i_target->getAttr<TARGETING::ATTR_CHIP_ID>();
}

// IMPORTANT:
// The ATTR_EC attribute will be synced from Hostboot to the BMC at some
// point during the IPL. It is possible that this information is needed
// before the sync occurs, in which case the value will return 0.
uint8_t __getChipEc(TARGETING::TargetPtr i_target)
{
    return i_target->getAttr<TARGETING::ATTR_EC>();
}

uint32_t __getChipIdEc(TARGETING::TargetPtr i_target)
{
    auto chipId = __getChipId(i_target);
    auto chipEc = __getChipEc(i_target);

    if (((0 == chipId) || (0 == chipEc)) &&
        (TARGETING::TYPE_PROC == getTrgtType(i_target)))
    {
        // There is a special case where the model/level attributes have not
        // been initialized in the devtree. This is possible on the epoch
        // IPL where an attention occurs before Hostboot is able to update
        // the devtree information on the BMC. It may is still possible to
        // get this information from chips with CFAM access (i.e. a
        // processor) via the CFAM chip ID register.

        uint32_t val = 0;
        if (0 == getCfam(i_target, 0x100a, val))
        {
            chipId = ((val & 0x0F0FF000) >> 12);
            chipEc = ((val & 0xF0000000) >> 24) | ((val & 0x00F00000) >> 20);
        }
    }

    return ((chipId & 0xffff) << 16) | (chipEc & 0xff);
}

void __addChip(std::vector<libhei::Chip>& o_chips,
               TARGETING::TargetPtr i_target, libhei::ChipType_t i_type)
{
    // Trace each chip for debug. It is important to show the type just in
    // case the model/EC does not exist. See note below.
    trace::inf("Chip found: type=0x%08" PRIx32 " chip=%s", i_type,
               getPath(i_target));

    if (0 == i_type)
    {
        // This is a special case. See the details in __getChipIdEC(). There
        // is nothing more we can do with this chip since we don't know what
        // it is. So ignore the chip for now.
    }
    else
    {
        o_chips.emplace_back(i_target, i_type);
    }
}

// Should ignore OCMBs that have been masked on the processor side of the bus.
bool __isMaskedOcmb(const libhei::Chip& i_chip)
{
    // Map of MCC target position to DSTL_FIR_MASK address.
    static const std::map<unsigned int, uint64_t> addrs = {
        {0, 0x08011842},  {1, 0x08011A42},  {2, 0x08011C42},  {3, 0x08011E42},
        {4, 0x09011842},  {5, 0x09011A42},  {6, 0x09011C42},  {7, 0x09011E42},
        {8, 0x0A011842},  {9, 0x0A011A42},  {10, 0x0A011C42}, {11, 0x0A011E42},
        {12, 0x0B011842}, {13, 0x0B011A42}, {14, 0x0B011C42}, {15, 0x0B011E42},
    };

    auto ocmb = getTrgt(i_chip);

    // Confirm this chip is an OCMB.
    if (TARGETING::TYPE_OCMB_CHIP != getTrgtType(ocmb))
    {
        return false;
    }

    // Get the connected MCC target on the processor chip.
    auto mcc = TARGETING::utils::getParentTarget(ocmb, TARGETING::TYPE_MCC);
    if (nullptr == mcc)
    {
        throw std::logic_error(
            "No parent MCC found for " + std::string{getPath(ocmb)});
    }

    // Read the associated DSTL_FIR_MASK.
    uint64_t val = 0;
    if (getScom(getParentChip(mcc), addrs.at(getUnitPos(mcc)), val))
    {
        // Just let this go. The SCOM code will log the error.
        return false;
    }

    // The DSTL_FIR has bits for each of the two memory channels on the MCC.
    auto chnlPos = getChipPos(ocmb) % 2;

    // Channel 0 => bits 1-4, channel 1 => bits 5-8.
    auto mask = (val >> (59 - (4 * chnlPos))) & 0xf;

    // Return true if the mask is set to all 1's.
    if (0xf == mask)
    {
        trace::inf("OCMB masked on processor side of bus: %s", getPath(ocmb));
        return true;
    }

    return false; // default
}

void getActiveChips(std::vector<libhei::Chip>& o_chips)
{
    o_chips.clear();

    // Iterate each hub.
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        // Active hubs only.
        if (!TARGETING::utils::isFunctional(hub))
            continue;

        // Add the hub to the list.
        __addChip(o_chips, hub, __getChipIdEc(hub));

        // Iterate the connected OCMBs, if they exist.
        TARGETING::TargetPtrList ocmbList =
            TARGETING::utils::getChildTargets(hub, TARGETING::TYPE_OCMB_CHIP);
        for (const auto& ocmb : ocmbList)
        {
            // Active OCMBs only.
            if (!TARGETING::utils::isFunctional(ocmb))
                continue;

            // Add the OCMB to the list.
            __addChip(o_chips, ocmb, __getChipIdEc(ocmb));
        }
    }

    // Ignore OCMBs that have been masked on the hub side of the bus.
    o_chips.erase(
        std::remove_if(o_chips.begin(), o_chips.end(), __isMaskedOcmb),
        o_chips.end());
}

//------------------------------------------------------------------------------

void getActiveHubChips(TARGETING::TargetPtrList& o_chips)
{
    o_chips.clear();

    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        if (!TARGETING::utils::isFunctional(hub))
            continue;

        o_chips.push_back(hub);
    }
}

//------------------------------------------------------------------------------

std::string getLocationCode(TARGETING::TargetPtr i_target)
{
    if (nullptr == i_target)
    {
        // Either the path is wrong or the attribute doesn't exist.
        return std::string{};
    }

    TARGETING::ATTR_LOCATION_CODE_type val;
    if (!i_target->tryGetAttr<TARGETING::ATTR_LOCATION_CODE>(val))
    {
        // Get the immediate parent in the devtree path and try again.
        auto& targetService = TARGETING::TargetService::instance();
        return getLocationCode(targetService.getParentOf(i_target));
    }

    // Attribute found.
    return std::string{val};
}

//------------------------------------------------------------------------------

std::vector<uint8_t> getPhysBinPath(TARGETING::TargetPtr i_target)
{
    if (nullptr != i_target)
    {
        TARGETING::EntityPath value;
        if (!i_target->tryGetAttr<TARGETING::ATTR_PHYS_PATH>(value))
        {
            // The attribute for this target does not exist. Get the
            // immediate parent in the devtree path and try again. Note that
            // if there is no parent target, nullptr will be returned and
            // that will be checked above.
            auto& targetService = TARGETING::TargetService::instance();
            return getPhysBinPath(targetService.getParentOf(i_target));
        }

        // Attribute was found. Copy the attribute array to the returned
        // vector. Note that the reason we return the vector instead of just
        // returning the array is because the array type and details only
        // exists in this specific configuration.
        const uint8_t* binVal = reinterpret_cast<const uint8_t*>(&value);
        std::vector<uint8_t> binPath(binVal,
                                     binVal + sizeof(TARGETING::EntityPath));
        return binPath;
    }

    // input target was null
    return std::vector<uint8_t>();
}

//------------------------------------------------------------------------------

} // namespace pdbg

} // namespace util
