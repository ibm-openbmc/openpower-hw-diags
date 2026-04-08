#pragma once

#include <analyzer/resolution.hpp>
#include <hei_main.hpp>
#include <nlohmann/json.hpp>

#include <map>

namespace analyzer
{
/**
 * @brief Manages the RAS data files and resolves service actions required for
 *        error signatures.
 */
class RasDataParser
{
  public:
    /** @brief Default constructor. */
    RasDataParser()
    {
        initDataFiles();
    }

    /** Define all RAS data flags that may be associated with a signature */
    enum RasDataFlags
    {
        SUE_SOURCE,
        SUE_SEEN,
        CS_POSSIBLE,
        RECOVERED_ERROR,
        INFORMATIONAL_ONLY,
        MNFG_INFORMATIONAL_ONLY,
        MASK_BUT_DONT_CLEAR,
        CRC_RELATED_ERR,
        CRC_ROOT_CAUSE,
        ODP_DATA_CORRUPT_SIDE_EFFECT,
        ODP_DATA_CORRUPT_ROOT_CAUSE,
        ATTN_FROM_OCMB,
    };

  private:
    /** @brief The RAS data files. */
    std::map<libhei::ChipType_t, nlohmann::json> iv_dataFiles;

  public:
    /**
     * @brief Returns a resolution for all the RAS actions needed for the given
     *        signature.
     * @param i_signature The target error signature.
     */
    std::shared_ptr<Resolution> getResolution(
        const libhei::Signature& i_signature);

    /**
     * @brief Initializes the signature list within the input isolation data
     *        with their appropriate flags based on the RAS data files.
     * @param i_signature The target error signature.
     * @param i_flag      The flag to check for
     * @return True if the flag is set for the given signature, else false.
     */
    bool isFlagSet(const libhei::Signature& i_signature,
                   const RasDataFlags i_flag) const;

    /**
     * @brief Returns of the version of the relevant RAS data file for the
     *        input signature.
     * @param i_signature The target error signature.
     * @return The version of the RAS data file.
     */
    unsigned int getVersion(const libhei::Signature& i_signature) const;

  private:
    /**
     * @brief Parses all of the RAS data JSON files and validates them against
     *        the associated schema.
     */
    void initDataFiles();

    /**
     * @brief  Parses a unit object in the given data file and returns the unit
     *         type and unit position.
     * @param  i_data The parsed RAS data file associated with the signature's
     *                chip type.
     * @param  i_name The name of the target unit.
     * @return A apir containing the unit type and unit position.
     */
    std::pair<TARGETING::TYPE, uint8_t> parseUnit(
        const nlohmann::json& i_data, const std::string& i_unitName);

    /**
     * @brief  Parses a signature in the given data file and returns a string
     *         representing the target action for the signature.
     * @param  i_data      The parsed RAS data file associated with the
     *                     signature's chip type.
     * @param  i_signature The target signature.
     * @return A string representing the target action for the signature.
     */
    std::string parseSignature(const nlohmann::json& i_data,
                               const libhei::Signature& i_signature) const;

    /**
     * @brief  Parses a bus object in the given data file and returns the bus
     *         type, unit type, and unit position.
     * @param  i_data The parsed RAS data file associated with the signature's
     *                chip type.
     * @param  i_name The name of the target bus.
     * @return A tuple containing the bus type, unit type, and unit position.
     */
    std::tuple<callout::BusType, TARGETING::TYPE, uint8_t> parseBus(
        const nlohmann::json& i_data, const std::string& i_name);

    /**
     * @brief  Parses an action in the given data file and returns the
     *         corresponding resolution.
     * @param  i_data   The parsed RAS data file associated with the signature's
     *                  chip type.
     * @param  i_action The target action to parse from the given RAS data.
     * @return A resolution (or nested resolutions) representing the given
     *         action.
     * @note   This function is called recursively because an action could
     *         reference another action. This function will maintain a stack of
     *         parsed actions and will assert that the same action cannot be
     *         parsed more than once in the recursion stack.
     */
    std::shared_ptr<Resolution> parseAction(const nlohmann::json& i_data,
                                            const std::string& i_action);

    /**
     * @brief  Returns a callout priority enum value for the given string.
     * @param  i_priority The priority string.
     * @return A callout priority enum value.
     */
    callout::Priority getPriority(const std::string& i_priority);
};

} // namespace analyzer
