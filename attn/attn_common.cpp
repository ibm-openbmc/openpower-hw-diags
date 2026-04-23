#include <attn/attn_common.hpp>
#include <attn/attn_handler.hpp>
#include <attn/attn_logging.hpp>
#include <sdbusplus/bus.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include <iomanip>
#include <iostream>
#include <map>

namespace attn
{

/** @brief Traces some regs for hostboot */
void addHbStatusRegs()
{
    // TODO - updates needed
    auto hub = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP)[0];

    uint32_t l_cfamData = 0xFFFFFFFF;
    uint64_t l_scomData1 = 0xFFFFFFFFFFFFFFFFull;
    uint64_t l_scomData2 = 0xFFFFFFFFFFFFFFFFull;
    uint32_t l_cfamAddr = 0x283C;
    uint64_t l_scomAddr1 = 0x4602F489;
    uint64_t l_scomAddr2 = 0x4602F487;

    if ((nullptr != hub))
    {
        // get first debug reg (CFAM)
        if (RC_SUCCESS != util::pdbg::getCfam(hub, l_cfamAddr, l_cfamData))
        {
            trace::err("cfam read error: 0x%08x", l_cfamAddr);
            l_cfamData = 0xFFFFFFFF;
        }

        // Get SCOM regs next (just 2 of them)
        if (RC_SUCCESS != util::pdbg::getScom(hub, l_scomAddr1, l_scomData1))
        {
            trace::err("scom read error: 0x%016" PRIx64 "", l_scomAddr1);
            l_scomData1 = 0xFFFFFFFFFFFFFFFFull;
        }

        if (RC_SUCCESS != util::pdbg::getScom(hub, l_scomAddr2, l_scomData2))
        {
            trace::err("scom read error: 0x%016" PRIx64 "", l_scomAddr2);
            l_scomData2 = 0xFFFFFFFFFFFFFFFFull;
        }
    }

    // Trace out the results here of all 3 regs
    trace::inf("HostBoot Reg:%08x Data:%08x Hub:00000000", l_cfamAddr,
               l_cfamData);
    trace::inf("HostBoot Reg:%08" PRIx64 " Data:%016" PRIx64 " Hub:00000000",
               l_scomAddr1, l_scomData1);
    trace::inf("HostBoot Reg:%08" PRIx64 " Data:%016" PRIx64 " Hub:00000000",
               l_scomAddr2, l_scomData2);

    return;

} // end addHbStatusRegs

/** @brief Check for recoverable errors present */
bool recoverableErrors()
{
    bool recoverableErrors = false; // assume no recoverable attentions

    auto hubList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        // Active hubs only.
        if (TARGETING::utils::isFunctional(hub))
        {
            uint32_t isr_val = 0xffffffff; // invalid isr value

            // get active attentions on processor
            if (RC_SUCCESS != util::pdbg::getCfam(hub, 0x1007, isr_val))
            {
                // log cfam read error
                trace::err("cfam read 0x1007 FAILED");
                eventAttentionFail(
                    (int)AttnSection::attnHandler | ATTN_PDBG_CFAM);
            }
            // check for invalid/stale value
            else if (0xffffffff == isr_val)
            {
                trace::err("cfam read 0x1007 INVALID");
                continue;
            }
            // check recoverable error status bit
            else if (0 != (isr_val & RECOVERABLE_ATTN))
            {
                recoverableErrors = true;
                break;
            }
        } // functional hub
    } // next hub chip

    return recoverableErrors;
}

/** @brief timesec less-than-equal-to compare */
bool operator<=(const timespec& lhs, const timespec& rhs)
{
    if (lhs.tv_sec == rhs.tv_sec)
        return lhs.tv_nsec <= rhs.tv_nsec;
    else
        return lhs.tv_sec <= rhs.tv_sec;
}

/** @brief sleep for n-seconds */
void sleepSeconds(const unsigned int seconds)
{
    auto count = seconds;
    struct timespec requested, remaining;

    while (0 < count)
    {
        requested.tv_sec = 1;
        requested.tv_nsec = 0;
        remaining = requested;

        while (-1 == nanosleep(&requested, &remaining))
        {
            // if not changing or implausible then abort
            if (requested <= remaining)
            {
                break;
            }

            // back to sleep
            requested = remaining;
        }
        count--;
    }
}

} // namespace attn
