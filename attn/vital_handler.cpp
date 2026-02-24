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
bool attemptSbeRecovery(int sbeInstance)
{
    // attempt sbe hreset and attention interrupt clear
    if (!util::pldm::hresetSbe(sbeInstance))
    {
        return false;
    }

    trace::inf("hreset completed");

    // try to clear attention interrupts
    clearAttnInterrupts();

    // loop through processors checking attention interrupts
    bool recovered = true;
    pdbg_target* procTarget;
    pdbg_for_each_class_target("proc", procTarget)
    {
        // active processors only
        if (PDBG_TARGET_ENABLED !=
            pdbg_target_probe(util::pdbg::getPibTrgt(procTarget)))
        {
            continue;
        }

        // get cfam is an fsi read
        pdbg_target* fsiTarget = util::pdbg::getFsiTrgt(procTarget);
        uint32_t int_val;

        // get attention interrupts on processor
        if (RC_SUCCESS == fsi_read(fsiTarget, 0x100b, &int_val))
        {
            if (int_val & SBE_ATTN)
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
 * @param procInstance - proc to check for attentions
 *
 * @pre pdbg target associated with proc instance is enabled for fsi access
 *
 * @return true if checkstop active false otherwise
 * */
bool checkstopActive(int procInstance)
{
    // get fsi target
    char path[16];
    sprintf(path, "/proc%d/fsi", procInstance);
    pdbg_target* fsiTarget = pdbg_target_from_path(nullptr, path);
    if (nullptr == fsiTarget)
    {
        trace::inf("fsi path or target not found");
        return false;
    }

    // check for active checkstop attention
    int r;
    uint32_t isr_val, isr_mask;

    isr_val = 0xffffffff;
    r = fsi_read(fsiTarget, 0x1007, &isr_val);
    if ((RC_SUCCESS != r) || (0xffffffff == isr_val))
    {
        trace::err("cfam 1007 read error");
        return false;
    }

    isr_mask = 0xffffffff;
    r = fsi_read(fsiTarget, 0x100d, &isr_mask);
    if ((RC_SUCCESS != r) || (0xffffffff == isr_mask))
    {
        trace::err("cfam 100d read error");
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
    int instance =
        pdbg_target_index(i_attention->getTarget()); // get processor number

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
