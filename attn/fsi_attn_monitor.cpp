#include <linux/ioctl.h>
#include <linux/types.h>

#include <attn/attn_handler.hpp>
#include <attn/fsi_attn_monitor.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

namespace attn
{

/* Structure for SCOM interrupt handling */
struct scom_interrupt
{
    __u32 comp_mask;
    __u32 true_mask;
};

#define FSI_SCOM_SET_INTERRUPT _IOW('s', 0x05, struct scom_interrupt)

int fsi_configure_scom_interrupt(int i_fd, uint32_t i_comp_mask,
                                 uint32_t i_true_mask)
{
    struct scom_interrupt si;
    int rc;

    // comp_mask and true_mask for the FSI_SCOM_SET_INTERRUPT here correspond to
    // the complement mask (0x100c) and true mask (0x100d) of fsi2pib.
    si.comp_mask = i_comp_mask;
    si.true_mask = i_true_mask;

    rc = ioctl(i_fd, FSI_SCOM_SET_INTERRUPT, &si);
    if (rc < 0)
        return -errno;

    return 0;
}

void configureFsi2Pib()
{
    // One "/dev/scom#" file will exist per hub.
    auto hubList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        // Active hubs only.
        if (!TARGETING::utils::isFunctional(hub))
            continue;

        uint32_t fapiPos = util::pdbg::getChipPos(hub);

        // The /dev/scom files start at /dev/scom1, so add one to the FAPI_POS
        // to get the correct position.
        char scomName[64];
        sprintf(scomName, "/dev/scom%d", fapiPos + 1);

        int fd = open(scomName, O_RDWR);

        if (fd < 0)
        {
            trace::err("failed to get file descriptor %s", scomName);
        }

        // FSI2PIB status:
        // bit 1 - CHIP_CS
        // bit 2 - SP_ATTN
        // bit 5 - any TAP chip event
        if (fsi_configure_scom_interrupt(fd, 0, 0x64000000))
        {
            trace::err("FsiAttnMonitor::configureFsiEvent - failure from "
                       "fsi_configure_scom_interrupt() for %s",
                       scomName);
            close(fd);
            return;
        }
    }
}

/** @brief Register a callback for FSI event */
void FsiAttnMonitor::scheduleFsiEvent(
    boost::asio::posix::stream_descriptor& i_eventDesc)
{
    i_eventDesc.async_wait(
        boost::asio::posix::stream_descriptor::wait_read,
        [this, &i_eventDesc](const boost::system::error_code& ec) {
            if (ec)
            {
                trace::err("FSI Async wait error: %s", ec.message().c_str());
            }
            else
            {
                trace::inf("Attention FSI active");
                handleFsiEvent(i_eventDesc); // FSI trigger detected
            }
        });
}

/** @brief Handle the FSI state change event */
void FsiAttnMonitor::handleFsiEvent(
    boost::asio::posix::stream_descriptor& i_eventDesc)
{
    attnHandler(iv_config);
    scheduleFsiEvent(i_eventDesc); // continue monitoring FSI
}

/** @brief Configure an FSI line for monitoring attention events */
void FsiAttnMonitor::configureFsiEvent()
{
    // One "/dev/scom#" file will exist per hub. Each one needs to be monitored
    // for attentions, so loop through all active hubs.
    auto hubList = TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);
    for (const auto& hub : hubList)
    {
        // Active hubs only.
        if (!TARGETING::utils::isFunctional(hub))
            continue;

        uint32_t fapiPos = util::pdbg::getChipPos(hub);

        // The /dev/scom files start at /dev/scom1, so add one to the FAPI_POS
        // to get the correct position.
        char scomName[64];
        sprintf(scomName, "/dev/scom%d", fapiPos + 1);

        int fd = open(scomName, O_RDWR);

        if (fd < 0)
        {
            trace::err("failed to get file descriptor %s", scomName);
            close(fd);
            continue;
        }

        // FSI2PIB status:
        // bit 1 - CHIP_CS
        // bit 2 - SP_ATTN
        // bit 5 - any TAP chip event
        if (0 != fsi_configure_scom_interrupt(fd, 0, 0x64000000))
        {
            trace::err("FsiAttnMonitor::configureFsiEvent: Failed to "
                       "configure scom interrupt for %s",
                       scomName);
            continue;
        }

        iv_streamDescriptors.push_back(
            std::make_unique<boost::asio::posix::stream_descriptor>(iv_io));
        iv_streamDescriptors.back()->assign(fd);

        scheduleFsiEvent(*iv_streamDescriptors.back());
    }

    iv_io.run();
}

} // namespace attn
