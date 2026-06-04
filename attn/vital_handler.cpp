#include <attn/attention.hpp>
#include <attn/attn_common.hpp>
#include <attn/attn_dump.hpp>
#include <attn/attn_handler.hpp>
#include <attn/attn_logging.hpp>
#include <sdbusplus/bus.hpp>
#include <util/dbus.hpp>
#include <util/pdbg.hpp>
#include <util/pldm.hpp>
#include <util/trace.hpp>

namespace attn
{
/*
 * @brief Request SBE hreset and try to clear sbe attentions
 *
 * @param[in] sbeInstance - sbe instance to hreset (0 based)
 *
 * @return true if hreset is successful and attentions cleared
 */
bool attemptSbeRecovery(uint32_t sbeInstance)
{
    // attempt sbe hreset and attention interrupt clear
    if (!util::pldm::hresetSbe(sbeInstance))
    {
        return false;
    }

    trace::inf("hreset completed");

    // try to clear attention interrupts
    clearAttnInterrupts();

    // loop through hubs checking attention interrupts
    bool recovered = true;
    auto hubList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        // active hubs only
        if (!TARGETING::utils::isFunctional(hub))
        {
            continue;
        }

        uint32_t int_val;
        // get attention interrupts on the hub
        if (RC_SUCCESS == util::pdbg::getCfam(hub, 0x100b, int_val))
        {
            if (int_val & SPPE_ATTN)
            {
                trace::err("sbe attention did not clear");
                recovered = false;
                break;
            }
        }
        else
        {
            // log cfam read error
            trace::err("cfam read error");
            recovered = false;
            break;
        }
    }

    if (recovered)
    {
        trace::inf("sbe attention cleared");
    }

    return recovered;
}

/**
 * @brief Check for active checkstop attention
 *
 * @param hubInstance - hub to check for attentions
 *
 * @pre pdbg target associated with hub instance is enabled for fsi access
 *
 * @return true if checkstop acive false otherwise
 * */
bool checkstopActive(uint32_t hubInstance)
{
    // get target
    auto hubList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    TARGETING::TargetPtr hubTrgt = nullptr;
    for (const auto& hub : hubList)
    {
        if (hubInstance == util::pdbg::getChipPos(hub))
        {
            hubTrgt = hub;
            break;
        }
    }
    if (nullptr == hubTrgt)
    {
        trace::inf("hub%d target not found", hubInstance);
        return false;
    }

    // check for active checkstop attention
    int rc;
    uint32_t isr_val, isr_mask;

    isr_val = 0xffffffff;
    rc = util::pdbg::getCfam(hubTrgt, 0x1007, isr_val);
    if ((RC_SUCCESS != rc) || (0xffffffff == isr_val))
    {
        trace::err("cfam 1007 read error on hub%d", hubInstance);
        return false;
    }

    isr_mask = 0xffffffff;
    rc = util::pdbg::getCfam(hubTrgt, 0x100d, isr_mask);
    if ((RC_SUCCESS != rc) || (0xffffffff == isr_mask))
    {
        trace::err("cfam 100d read error on hub%d", hubInstance);
        return false;
    }

    return activeAttn(isr_val, isr_mask, CHECKSTOP_ATTN);
}

/**
 * @brief Handle SBE vital attention
 *
 * @param i_attention - attention object
 *
 * @return non-zero if attention was not successfully handled
 */
int handleVital(Attention* i_attention)
{
    trace::inf("vital handler started");

    // TODO - SPPE vital handling updates for P12
    trace::inf("TODO: SPPE vital handling currently disabled");
    return RC_NOT_HANDLED;

    // if vital handling disabled
    if (false == (i_attention->getConfig()->getFlag(enVital)))
    {
        trace::inf("vital handling disabled");
        return RC_NOT_HANDLED;
    }

    // if power fault then we don't do anything
    sleepSeconds(POWER_FAULT_WAIT);
    if (util::dbus::powerFault())
    {
        trace::inf("power fault was reported");
        return RC_SUCCESS;
    }

    // if no checkstop and host is running
    // get hub number
    uint32_t instance = TARGETING::utils::getPosition(i_attention->getTarget());

    if (!checkstopActive(instance) &&
        util::dbus::HostRunningState::Started == util::dbus::hostRunningState())
    {
        // attempt to recover the sbe
        if (attemptSbeRecovery(instance))
        {
            eventVital(levelPelInfo);
            return RC_SUCCESS;
        }
    }

    // host not running, checkstop active or recovery failed
    auto pelId = eventVital(levelPelError);
    requestDump(pelId, DumpParameters{0, DumpType::SBE});
    util::dbus::transitionHost(util::dbus::HostState::Quiesce);

    return RC_SUCCESS;
}

} // namespace attn
