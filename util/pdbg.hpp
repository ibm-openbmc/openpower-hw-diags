#pragma once

// PHAL targeting includes
#include <hwaccess/hw_access_intf.H>
#include <hwaccess/hw_access_util.H>
#include <target_utils.H>
#include <targeting/target.H>
#include <targetsvc/target_service.H>

#include <analyzer/callout.hpp>

#include <string>
#include <vector>

// Forward reference to avoid pulling the libhei library into everything that
// includes this header.
namespace libhei
{
class Chip;
}

namespace util
{

namespace pdbg
{

/** Chip target types. */
// TODO - remove? - use phal TARGETING::TYPE directly?
enum TargetType_t : uint8_t
{
    TYPE_DIMM = 0x03,
    TYPE_PROC = 0x05,
    TYPE_CORE = 0x07,
    TYPE_NX = 0x1e,
    TYPE_EQ = 0x23,
    TYPE_PEC = 0x2d,
    TYPE_PHB = 0x2e,
    TYPE_MC = 0x44,
    TYPE_IOLINK = 0x47,
    TYPE_OMI = 0x48,
    TYPE_MCC = 0x49,
    TYPE_OMIC = 0x4a,
    TYPE_OCMB = 0x4b,
    TYPE_MEM_PORT = 0x4c,
    TYPE_NMMU = 0x4f,
    TYPE_PAU = 0x50,
    TYPE_IOHS = 0x51,
    TYPE_PAUC = 0x52,
};

/** @return The target associated with the given chip. */
TARGETING::TargetPtr getTrgt(const libhei::Chip& i_chip);

// TODO - update to take type and pos instead of devtree path
/** @return The target associated with the given devtree path. */
TARGETING::TargetPtr getTrgt(const std::string& i_path);

/** @return A string representing the given target's physical path. */
const std::string getPath(TARGETING::TargetPtr i_target);

/** @return A string representing the given chip's physical path. */
const std::string getPath(const libhei::Chip& i_chip);

/** @return The absolute position of the given target. */
uint32_t getChipPos(TARGETING::TargetPtr i_target);

/** @return The absolute position of the given chip. */
uint32_t getChipPos(const libhei::Chip& i_chip);

/** @return The unit position of a target within a chip. */
uint8_t getUnitPos(TARGETING::TargetPtr i_target);

/** @return The target type of the given target. */
uint8_t getTrgtType(TARGETING::TargetPtr i_target);

/** @return The target type of the given chip. */
uint8_t getTrgtType(const libhei::Chip& i_chip);

/** @return The parent chip target of the given unit target. */
TARGETING::TargetPtr getParentChip(TARGETING::TargetPtr i_unitTarget);

/** @return The parent hub chip target of the given target. */
TARGETING::TargetPtr getParentHub(TARGETING::TargetPtr i_target);

/** @return The unit target within chip of the given unit type and position
 *          relative to the chip. */
TARGETING::TargetPtr getChipUnit(TARGETING::TargetPtr i_parentChip,
                                 TARGETING::TYPE i_unitType, uint8_t i_unitPos);

/**
 * @return The connected target on the other side of the given bus.
 * @param  i_rxTarget The target on the receiving side (RX) of the bus.
 * @param  i_busType  The bus type.
 */
TARGETING::TargetPtr getConnectedTarget(
    TARGETING::TargetPtr i_rxTarget,
    const analyzer::callout::BusType& i_busType);

/**
 * @return The pib target associated with the given proc target.
 * @note   Will assert the given target is a proc target.
 * @note   Will assert the returned pib target it not nullptr.
 */
TARGETING::TargetPtr getPibTrgt(TARGETING::TargetPtr i_procTrgt);

/**
 * @return The fsi target associated with the given proc target.
 * @note   Will assert the given target is a proc target.
 * @note   Will assert the returned fsi target it not nullptr.
 */
TARGETING::TargetPtr getFsiTrgt(TARGETING::TargetPtr i_procTrgt);

/**
 * @brief  Reads a SCOM register.
 * @param  i_trgt Given target.
 * @param  i_addr Given address.
 * @param  o_val  The returned value of the register.
 * @return 0 if successful, non-0 otherwise.
 * @note   Will assert the given target is a proc target.
 */
int getScom(TARGETING::TargetPtr i_target, uint64_t i_addr, uint64_t& o_val);

/**
 * @brief  Reads a CFAM FSI register.
 * @param  i_trgt Given target.
 * @param  i_addr Given address.
 * @param  o_val  The returned value of the register.
 * @return 0 if successful, non-0 otherwise.
 * @note   Will assert the given target is a proc target.
 */
int getCfam(TARGETING::TargetPtr i_target, uint32_t i_addr, uint32_t& o_val);

/**
 * @brief  Writes a CFAM FSI register.
 * @param  i_trgt Given target.
 * @param  i_addr Given address.
 * @param  i_val  Value to write to the register.
 * @return 0 if successful, non-0 otherwise.
 * @note   Will assert the given target is a proc target.
 */
int putCfam(TARGETING::TargetPtr i_target, uint32_t i_addr, uint32_t i_val);

/**
 * @brief Returns the list of all active chips in the system.
 * @param o_chips The returned list of chips.
 */
void getActiveChips(std::vector<libhei::Chip>& o_chips);

/**
 * @brief Returns the list of all active processor chips in the system.
 * @param o_chips The returned list of chips.
 */
void getActiveProcessorChips(TARGETING::TargetPtrList& o_chips);

/**
 * @return The primary Hub chip (i.e. the hub connected to the BMC).
 */
TARGETING::TargetPtr getPrimaryHub();

/**
 * @return A string containing the FRU location code of the given chip. An empty
 *         string indicates the target was null or the attribute does not exist
 *         for this target.
 * @note   This function requires PHAL APIs that are only available in certain
 *         environments. If they do not exist the devtree path of the target is
 *         returned.
 */
std::string getLocationCode(TARGETING::TargetPtr i_target);

/**
 * @return A vector of bytes representing the numerical values of the physical
 *         device path (entity path) of the given target. An empty vector
 *         indicates the target was null or the attribute does not exist for
 *         this target or any parent targets along the device tree path.
 * @note   This function requires PHAL APIs that are only available in certain
 *         environments. If they do not exist, an empty vector is returned.
 */
std::vector<uint8_t> getPhysBinPath(TARGETING::TargetPtr i_target);

/**
 * @brief  Uses an SBE chip-op to query if there has been an LPC timeout.
 * @return True, if there was an LPC timeout. False, otherwise.
 */
bool queryLpcTimeout(TARGETING::TargetPtr i_target);

} // namespace pdbg

} // namespace util
