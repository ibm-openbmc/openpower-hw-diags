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
    TARGETING::TargetPtr bootHub = util::pdbg::getBootHub();

    uint32_t l_cfamData = 0xFFFFFFFF;
    uint64_t l_scomData1 = 0xFFFFFFFFFFFFFFFFull;
    uint64_t l_scomData2 = 0xFFFFFFFFFFFFFFFFull;
    constexpr uint32_t l_cfamAddr = 0x283C;
    constexpr uint64_t l_scomAddr1 = 0x4602F489;
    constexpr uint64_t l_scomAddr2 = 0x4602F487;
    uint32_t bootHubPos = 0;
    uint32_t computePos = 0;

    if (nullptr != bootHub)
    {
        bootHubPos = util::pdbg::getChipPos(bootHub);

        // Get debug CFAM reg from the boot hub.
        if (RC_SUCCESS != util::pdbg::getCfam(bootHub, l_cfamAddr, l_cfamData))
        {
            trace::err("cfam read error: 0x%08x", l_cfamAddr);
            l_cfamData = 0xFFFFFFFF;
        }

        // Get SCOM regs from compute chips under the boot hub. Once one
        // register is non-zero, use both values from that compute chip.
        auto computeList =
            TARGETING::utils::getFuctionalComputeChipsFromHub(bootHub);

        for (const auto& compute : computeList)
        {
            if (!TARGETING::utils::isFunctional(compute))
            {
                continue;
            }

            uint64_t scomData1 = 0;
            uint64_t scomData2 = 0;

            if (RC_SUCCESS !=
                util::pdbg::getScom(compute, l_scomAddr1, scomData1))
            {
                trace::err("scom read error: 0x%016" PRIx64 "", l_scomAddr1);
                scomData1 = 0;
            }

            if (RC_SUCCESS !=
                util::pdbg::getScom(compute, l_scomAddr2, scomData2))
            {
                trace::err("scom read error: 0x%016" PRIx64 "", l_scomAddr2);
                scomData2 = 0;
            }

            if ((0 != scomData1) || (0 != scomData2))
            {
                l_scomData1 = scomData1;
                l_scomData2 = scomData2;
                computePos = util::pdbg::getChipPos(compute);
                break;
            }
        }
    }

    // Trace out the results here of all 3 regs.
    trace::inf("HostBoot Reg:%08" PRIx64 " Data:%08" PRIx64 " Hub:%08" PRIx64,
               l_cfamAddr, l_cfamData, bootHubPos);
    trace::inf("HostBoot Reg:%08" PRIx64 " Data:%016" PRIx64
               " Compute:%08" PRIx64,
               l_scomAddr1, l_scomData1, computePos);
    trace::inf("HostBoot Reg:%08" PRIx64 " Data:%016" PRIx64
               " Compute:%08" PRIx64,
               l_scomAddr2, l_scomData2, computePos);

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
