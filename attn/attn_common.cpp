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
    /* TODO - remove or update for P12?
    // Only do this for P10 systems

    // We only need this for PRIMARY processor
    pdbg_target* pibTarget = pdbg_target_from_path(nullptr, "/proc0/pib");
    pdbg_target* fsiTarget = pdbg_target_from_path(nullptr, "/proc0/fsi");

    uint32_t l_cfamData = 0xFFFFFFFF;
    uint64_t l_scomData1 = 0xFFFFFFFFFFFFFFFFull;
    uint64_t l_scomData2 = 0xFFFFFFFFFFFFFFFFull;
    uint32_t l_cfamAddr = 0x283C;
    uint64_t l_scomAddr1 = 0x4602F489;
    uint64_t l_scomAddr2 = 0x4602F487;

    if ((nullptr != fsiTarget) && (nullptr != pibTarget))
    {
        // get first debug reg (CFAM)
        if (RC_SUCCESS != fsi_read(fsiTarget, l_cfamAddr, &l_cfamData))
        {
            trace::err("cfam read error: 0x%08x", l_cfamAddr);
            l_cfamData = 0xFFFFFFFF;
        }

        // Get SCOM regs next (just 2 of them)
        if (RC_SUCCESS != pib_read(pibTarget, l_scomAddr1, &l_scomData1))
        {
            trace::err("scom read error: 0x%016" PRIx64 "", l_scomAddr1);
            l_scomData1 = 0xFFFFFFFFFFFFFFFFull;
        }

        if (RC_SUCCESS != pib_read(pibTarget, l_scomAddr2, &l_scomData2))
        {
            trace::err("scom read error: 0x%016" PRIx64 "", l_scomAddr2);
            l_scomData2 = 0xFFFFFFFFFFFFFFFFull;
        }
    }

    // Trace out the results here of all 3 regs
    // (Format should resemble FSP: HostBoot Reg:0000283C  Data:AA801504
    // 00000000  Proc:00050001 )
    trace::inf("HostBoot Reg:%08x Data:%08x Proc:00000000", l_cfamAddr,
               l_cfamData);
    trace::inf("HostBoot Reg:%08" PRIx64 " Data:%016" PRIx64 " Proc:00000000",
               l_scomAddr1, l_scomData1);
    trace::inf("HostBoot Reg:%08" PRIx64 " Data:%016" PRIx64 " Proc:00000000",
               l_scomAddr2, l_scomData2);
    */

    return;

} // end addHbStatusRegs

/** @brief Capture some scratch registers for PRD */
void addPrdScratchRegs(std::vector<util::FFDCFile>& /*o_files*/)
{
    /* TODO - update for P12
    // Get primary processor FSI target for CFAM reads
    pdbg_target* fsiTarget = pdbg_target_from_path(nullptr, "/proc0/fsi");

    if (nullptr == fsiTarget)
    {
        trace::err("error getting scratch register target");
    }
    else
    {
        uint32_t chipId = 0;
        uint32_t signatureId = 0;

        // get scratch register 9 (CFAM)
        if (RC_SUCCESS != fsi_read(fsiTarget, 0x2980, &chipId))
        {
            trace::err("error reading scratch register 9");
            chipId = 0;
        }

        // get scratch register 10 (CFAM)
        if (RC_SUCCESS != fsi_read(fsiTarget, 0x2981, &signatureId))
        {
            trace::err("error reading scratch register 10");
            signatureId = 0;
        }

        // Add data to traces and create user data section
        if (0 != chipId || 0 != signatureId)
        {
            // trace scratch register data
            trace::inf("PRD scratch Proc0, Chip ID: %08x, Signature ID: %08x",
                       chipId, signatureId);

            // create ffdc data for user data section
            try
            {
                util::FFDCFile file{util::FFDCFormat::Text};
                int fd = file.getFileDescriptor();
                char buffer[150];
                int len = sprintf(buffer,
                                  "Scratch Register Error Signature\n"
                                  "Processor            : 0\n"
                                  "Chip ID              : %08x\n"
                                  "Signature ID         : %08x\n",
                                  chipId, signatureId);
                if (write(fd, buffer, len) < 0)
                {
                    trace::err("error writing scratch register user data");
                }
                else
                {
                    o_files.push_back(std::move(file));
                }
            }
            catch (const std::exception& e)
            {
                trace::err(
                    "exception when creating scratch register user data");
                trace::inf(e.what());
            }
        }
    }*/

    return;
}

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
