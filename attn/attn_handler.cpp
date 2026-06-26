#include <config.h>

#include <analyzer/analyzer_main.hpp>
#include <attn/attn-event.hpp>
#include <attn/attn_common.hpp>
#include <attn/attn_config.hpp>
#include <attn/attn_dbus.hpp>
#include <attn/attn_handler.hpp>
#include <attn/attn_logging.hpp>
#include <attn/bp_handler.hpp>
#include <attn/ti_handler.hpp>
#include <attn/vital_handler.hpp>
#include <util/dbus.hpp>
#include <util/ffdc_file.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include <algorithm>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>

namespace attn
{
/**
 * @brief Handle checkstop attention
 *
 * @param i_event An attention event.
 * @return 0 indicates that the checkstop attention was successfully handled
 *         1 indicates that the checkstop attention was NOT successfully
 *           handled.
 */
int handleCheckstop(Event* i_event);

/**
 * @brief Handle special attention
 *
 * @param i_event An attention event.
 * @return 0 indicates that the special attention was successfully handled
 *         1 indicates that the special attention was NOT successfully handled
 */
int handleSpecial(Event* i_event);

/** @brief Handle phal sbe exception */
/*
void phalSbeExceptionHandler(openpower::phal::exception::SbeError& e,
                             uint32_t chipPosition, uint32_t command);
*/

/** @brief Get static TI info data based on host state */
void getStaticTiInfo(uint8_t*& tiInfoPtr);

/** @brief Check if TI info data is valid */
bool tiInfoValid(uint8_t* tiInfo);

/**
 * @brief Adds connected compute chip attentions to the attention list.
 * @param i_hub The target hub chip.
 * @param i_config Attention handler configuration.
 * @param io_activeAttns The list of active attentions.
 */
void getComputeAttns(TARGETING::TargetPtr i_hub, Config* i_config,
                     std::vector<Event>& io_activeAttns)
{
    // TODO: Instead of iterating all compute chips. Look at the TAP_STATUS
    // (fsi: 2950) and TAP_MASK (fsi: 2954) registers to determine which compute
    // chips actually generated the attentions.

    TARGETING::TargetPtrList computeList =
        TARGETING::utils::getFuctionalComputeChipsFromHub(i_hub);

    // Look for active attentions on each functional compute chip connected to
    // this hub chip.
    for (const auto& compute : computeList)
    {
        trace::inf("compute: %u", TARGETING::utils::getPosition(compute));

        // Get the current FSI2PIB status.
        uint32_t isr_val;
        if (RC_SUCCESS != util::pdbg::getCfam(compute, 0x1007, isr_val))
        {
            trace::err("cfam read 0x1007 FAILED");
            eventAttentionFail((int)AttnSection::attnHandler | ATTN_PDBG_CFAM);
            continue;
        }
        trace::inf("cfam 0x1007 = 0x%08x", isr_val);

        // Get the current FSI2PIB mask.
        uint32_t isr_mask;
        if (RC_SUCCESS != util::pdbg::getCfam(compute, 0x100d, isr_mask))
        {
            trace::err("cfam read 0x100d FAILED");
            eventAttentionFail((int)AttnSection::attnHandler | ATTN_PDBG_CFAM);
            continue;
        }
        trace::inf("cfam 0x100d = 0x%08x", isr_mask);

        // TODO: The FSI attention driver currently clears the true
        // mask for any attention it responds to. This is, in
        // theory, to prevent attention flooding. This means, at the
        // moment, the true mask will not be set here. This may
        // cause an issue with breakpoints which should be the only
        // type of attention hw-diags is handling more than one of.
        // Either the mask needs to not be cleared if unnecessary or
        // hw-diags needs to reset the true mask somehow.
        // For now, to workaround this, we'll just assume the mask
        // is set to what we expect here
        isr_mask = FSI2PIB_BMC_ATTNS;

        if (activeAttn(isr_val, isr_mask, FSI2PIB_CHIP_CS))
        {
            // Note: Use hub as target, not compute chip
            io_activeAttns.emplace_back(Event::PRI_CHECKSTOP, handleCheckstop,
                                        i_hub, i_config);
        }

        if (activeAttn(isr_val, isr_mask, FSI2PIB_SPECIAL))
        {
            // Note: Use hub as target, not compute chip
            io_activeAttns.emplace_back(Event::PRI_SPECIAL, handleSpecial,
                                        i_hub, i_config);
        }
    }
}

/**
 * @brief The main attention handler logic
 *
 * @param i_breakpoints true = breakpoint special attn handling enabled
 */
void attnHandler(Config* i_config)
{
    // Check if enClrAttnIntr is enabled
    if (i_config->getFlag(enClrAttnIntr))
    {
        // Clear attention interrupts that may still be active (MPIPL)
        clearAttnInterrupts();
    }

    trace::inf("Attention handler started");

    // Keep a list of all active attentions.
    std::vector<Event> active_attentions;

    // Look for active attentions on each functional hub chip.
    auto hubList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        if (!TARGETING::utils::isFunctional(hub))
        {
            continue;
        }

        trace::inf("hub: %u", TARGETING::utils::getPosition(hub));

        // Get the current FSI2PIB status.
        uint32_t isr_val;
        if (RC_SUCCESS != util::pdbg::getCfam(hub, 0x1007, isr_val))
        {
            trace::err("cfam read 0x1007 FAILED");
            eventAttentionFail((int)AttnSection::attnHandler | ATTN_PDBG_CFAM);
            continue;
        }
        trace::inf("cfam 0x1007 = 0x%08x", isr_val);

        // Get the current FSI2PIB mask.
        uint32_t isr_mask;
        if (RC_SUCCESS != util::pdbg::getCfam(hub, 0x100d, isr_mask))
        {
            trace::err("cfam read 0x100d FAILED");
            eventAttentionFail((int)AttnSection::attnHandler | ATTN_PDBG_CFAM);
            continue;
        }
        trace::inf("cfam 0x100d = 0x%08x", isr_mask);

        // TODO: The FSI attention driver currently clears the true
        // mask for any attention it responds to. This is, in
        // theory, to prevent attention flooding. This means, at the
        // moment, the true mask will not be set here. This may
        // cause an issue with breakpoints which should be the only
        // type of attention hw-diags is handling more than one of.
        // Either the mask needs to not be cleared if unnecessary or
        // hw-diags needs to reset the true mask somehow.
        // For now, to workaround this, we'll just assume the mask
        // is set to what we expect here
        isr_mask = FSI2PIB_BMC_ATTNS;

        if (activeAttn(isr_val, isr_mask, FSI2PIB_CHIP_CS))
        {
            active_attentions.emplace_back(Event::PRI_CHECKSTOP,
                                           handleCheckstop, hub, i_config);
        }

        if (activeAttn(isr_val, isr_mask, FSI2PIB_SPECIAL))
        {
            active_attentions.emplace_back(Event::PRI_SPECIAL, handleSpecial,
                                           hub, i_config);
        }

        if (activeAttn(isr_val, isr_mask, FSI2PIB_SPPE_ATTN))
        {
            active_attentions.emplace_back(Event::PRI_SPPE_ATTN, handleVital,
                                           hub, i_config);
        }

        // Look for active attentions on each connected compute chip. Do this
        // after querying the other attentions on this hub chip. It makes the
        // traces easier to read.
        if (activeAttn(isr_val, isr_mask, FSI2PIB_COMPUTE_ATTN))
        {
            getComputeAttns(hub, i_config, active_attentions);
        }
    }

    // By converting the list to a heap the highest priority attentions are
    // moved to the front of the list.
    if (!std::is_heap(active_attentions.begin(), active_attentions.end()))
    {
        std::make_heap(active_attentions.begin(), active_attentions.end());
    }

    // Handle each attention in priority order until one is handled or all are
    // attempted.
    while (!active_attentions.empty())
    {
        // The highest priority attention is at the front of the list.
        if (RC_SUCCESS == active_attentions.front().handle())
        {
            break; // nothing more to do.
        }

        // Move this attention to back of list and remove it.
        std::pop_heap(active_attentions.begin(), active_attentions.end());
        active_attentions.pop_back();
    }
}

/**
 * @brief Handle checkstop attention
 *
 * @param i_event An attention event.
 * @return 0 indicates that the checkstop attention was successfully handled
 *         1 indicates that the checkstop attention was NOT successfully
 *           handled.
 */
int handleCheckstop(Event* i_event)
{
    int rc = RC_SUCCESS; // assume checkstop handled

    trace::inf("checkstop handler started");

    // capture some additional data for logs/traces
    addHbStatusRegs();

    // if checkstop handling enabled, handle checkstop attention
    if (false == (i_event->getConfig()->getFlag(enCheckstop)))
    {
        trace::inf("Checkstop handling disabled");
    }
    else
    {
        // check for power fault before starting analyses
        sleepSeconds(POWER_FAULT_WAIT);
        if (!util::dbus::powerFault())
        {
            // Look for any attentions found in hardware. This will generate and
            // commit a PEL if any errors are found.
            DumpParameters dumpParameters;
            auto logid = analyzer::analyzeHardware(
                analyzer::AnalysisType::SYSTEM_CHECKSTOP, dumpParameters);
            if (0 == logid)
            {
                // A PEL should exist for a checkstop attention.
                rc = RC_ANALYZER_ERROR;
            }
            else
            {
                requestDump(logid, dumpParameters);
                util::dbus::transitionHost(util::dbus::HostState::Quiesce);
            }
        }
    }

    return rc;
}

/**
 * @brief Handle special attention
 *
 * @param i_event An attention event.
 * @return 0 indicates that the special attention was successfully handled
 *         1 indicates that the special attention was NOT successfully handled
 */
int handleSpecial(Event* i_event)
{
    int rc = RC_SUCCESS; // assume special attention handled

    // The TI info chipop will give us a pointer to the TI info data
    uint8_t* tiInfo = nullptr; // ptr to TI info data

    // TODO - reenable
    // uint32_t tiInfoLen = 0;       // length of TI info data
    TARGETING::TargetPtr attnHub = i_event->getTarget(); // hub with attention

    bool tiInfoStatic = false; // assume TI info was provided (not created)

    // need hub target to get dynamic TI info
    if (nullptr != attnHub)
    {
        trace::inf("using libphal to get TI info");

        // phal library uses hub target for get ti info
        if (TARGETING::utils::isFunctional(attnHub))
        {
            /* TODO - new TI info function?
            try
            {
                // get dynamic TI info
                openpower::phal::sbe::getTiInfo(attnHub, &tiInfo, &tiInfoLen);
            }
            catch (openpower::phal::exception::SbeError& sbeError)
            {
                // library threw an exception
                // note: phal::sbe::getTiInfo = command class | command ==
                // 0xa900 | 0x04 = 0xa904. The sbe fifo command class and
                // commands are defined in the sbefifo library source code
                // but do not seem to be exported/installed for consumption
                // externally.
                uint32_t procNum = TARGETING::utils::getPosition(attnHub);
                // TODO - update?
                phalSbeExceptionHandler(sbeError, procNum, 0xa904);
            }*/
        }
        /* TODO - remove?
        trace::inf("using libpdbg to get TI info");

        if (nullptr != tiInfoTarget)
        {
            if (TARGETING::utils::isFunctional(tiInfoTarget))
            {
                // TODO - update interface?
                // sbe_mpipl_get_ti_info(tiInfoTarget, &tiInfo, &tiInfoLen);
            }
        }*/
    }

    // dynamic TI info is not available
    if (nullptr == tiInfo)
    {
        trace::inf("TI info data ptr is invalid");
        getStaticTiInfo(tiInfo);
        tiInfoStatic = true;
    }

    // check TI info for validity and handle
    if (true == tiInfoValid(tiInfo))
    {
        // TI info is valid handle TI if support enabled
        if (true == (i_event->getConfig()->getFlag(enTerminate)))
        {
            // Call TI special attention handler
            rc = tiHandler((TiDataArea*)tiInfo);
        }
    }
    else
    {
        trace::inf("TI info NOT valid");

        // if configured to handle TI as default special attention
        if (i_event->getConfig()->getFlag(dfltTi))
        {
            // TI handling may be disabled
            if (true == (i_event->getConfig()->getFlag(enTerminate)))
            {
                // Call TI special attention handler
                rc = tiHandler((TiDataArea*)tiInfo);
            }
        }
        // configured to handle breakpoint as default special attention
        else
        {
            // breakpoint handling may be disabled
            if (true == (i_event->getConfig()->getFlag(enBreakpoints)))
            {
                // Call the breakpoint special attention handler
                rc = bpHandler();
            }
        }
    }

    // release TI data buffer if not ours
    if (false == tiInfoStatic)
    {
        // sanity check
        if (nullptr != tiInfo)
        {
            free(tiInfo);
        }
    }

    // trace non-successful exit condition
    if (RC_SUCCESS != rc)
    {
        trace::inf("Special attn not handled");
    }

    return rc;
}

/** @brief Determine if attention is active and not masked */
bool activeAttn(uint32_t i_val, uint32_t i_mask, Fsi2PibAttn_t i_attn)
{
    bool rc = false; // assume attn masked and/or inactive

    // if attention active
    if (0 != (i_val & i_attn))
    {
        std::string msg;

        switch (i_attn)
        {
            case FSI2PIB_CHIP_CS:
                msg = "Checkstop attn";
                break;
            case FSI2PIB_SPECIAL:
                msg = "Special attn";
                break;
            case FSI2PIB_COMPUTE_ATTN:
                msg = "Compute attn";
                break;
            case FSI2PIB_SPPE_ATTN:
                msg = "SPPE attn";
                break;
            default:
                msg = "Unknown attn";
        }

        // see if attention is masked
        if (0 != (i_mask & i_attn))
        {
            rc = true; // attention active and not masked
        }
        else
        {
            msg += " masked";
        }

        trace::inf(msg.c_str()); // commit trace stream
    }

    return rc;
}

/**
 * @brief Handle phal sbe exception
 *
 * @param[in] e - exception object
 * @param[in] procNum - processor number associated with sbe exception
 */
/* TODO - update and reenable
void phalSbeExceptionHandler(openpower::phal::exception::SbeError& sbeError,
                             uint32_t chipPosition, uint32_t command)
{
    trace::err("Attention handler phal exception handler");

    // Trace exception details
    trace::err(sbeError.what());

    // Create event log entry with SBE FFDC data if provided
    auto fd = sbeError.getFd();
    if (fd > 0)
    {
        trace::err("SBE FFDC data is available");

        // FFDC parser expects chip position and command (command class |
        // command) to be in the additional data.
        std::map<std::string, std::string> additionalData = {
            {"SRC6", std::to_string((chipPosition << 16) | command)}};

        // See phosphor-logging/extensions/openpower-pels/README.md, "Self
        // Boot Engine(SBE) First Failure Data Capture(FFDC)" - SBE FFDC
        // file type is 0xCB, version is 0x01.
        std::vector<util::FFDCTuple> ffdc{util::FFDCTuple{
            util::FFDCFormat::Custom, static_cast<uint8_t>(0xCB),
            static_cast<uint8_t>(0x01), fd}};

        // Create event log entry with FFDC data
        util::dbus::createPel("org.open_power.Processor.Error.SbeChipOpFailure",
                              levelPelError, additionalData, ffdc);
    }
}
*/

/**
 * @brief Get static TI info data based on host state
 *
 * @param[out] tiInfo - pointer to static TI info data
 */
void getStaticTiInfo(uint8_t*& tiInfo)
{
    util::dbus::HostRunningState runningState = util::dbus::hostRunningState();

    // assume host state is unknown
    std::string stateString = "host state unknown";

    if ((util::dbus::HostRunningState::Started == runningState) ||
        (util::dbus::HostRunningState::Unknown == runningState))
    {
        if (util::dbus::HostRunningState::Started == runningState)
        {
            stateString = "host started";
        }
        tiInfo = (uint8_t*)defaultPhypTiInfo;
    }
    else
    {
        stateString = "host not started";
        tiInfo = (uint8_t*)defaultHbTiInfo;
    }

    // trace host state
    trace::inf(stateString.c_str());
}

/**
 * @brief Check if TI info data is valid
 *
 * @param[in] tiInfo - pointer to TI info data
 * @return true if TI info data is valid, else false
 */
bool tiInfoValid(uint8_t* tiInfo)
{
    bool tiInfoValid = false; // assume TI info invalid

    // TI info data[0] non-zero indicates TI info valid (usually)
    if ((nullptr != tiInfo) && (0 != tiInfo[0]))
    {
        TiDataArea* tiDataArea = (TiDataArea*)tiInfo;

        // trace a few known TI data area values
        trace::inf("TI data command = 0x%02x", tiDataArea->command);

        // Another check for valid TI Info since it has been seen that
        // tiInfo[0] != 0 but TI info is not valid
        if (0xa1 == tiDataArea->command)
        {
            tiInfoValid = true;

            // trace some more data since TI info appears valid
            trace::inf("TI data term-type = 0x%02x",
                       tiDataArea->hbTerminateType);

            trace::inf("TI data SRC format = 0x%02x", tiDataArea->srcFormat);

            trace::inf("TI data source = 0x%02x", tiDataArea->source);
        }
    }

    return tiInfoValid;
}

/**
 * @brief Clear attention interrupts.
 *
 * The FSI2PIB_INTERRUPT register (fsi: 0x100B) is a sticky version of the
 * FSI2PIB_STATUS register (fsi: 0x1007), which must be clear to allow new
 * interrupt to flow. This function blindly clears all relevant attention bits
 * and expects persistent attentions to retrigger the interrupt bits.
 */
void clearAttnInterrupts()
{
    trace::inf("Clearing attention interrupts");

    auto hubList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        if (!TARGETING::utils::isFunctional(hub))
        {
            continue;
        }

        uint32_t int_val;
        if (RC_SUCCESS != util::pdbg::getCfam(hub, 0x100b, int_val))
        {
            trace::err("cfam read 0x100b FAILED");
            continue;
        }

        trace::inf("cfam 0x100b = 0x%08x", int_val);

        // Clear all attentions regardless if we are monitoring them.
        int_val &= ~FSI2PIB_ALL_ATTNS;

        if (RC_SUCCESS != util::pdbg::putCfam(hub, 0x100b, int_val))
        {
            trace::err("cfam write 0x100b FAILED");
            continue;
        }
    }
}

} // namespace attn
