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

int fsi_configure_scom_interrupt(int fd, uint32_t comp_mask, uint32_t true_mask)
{
    struct scom_interrupt si;
    int rc;

    // comp_mask and true_mask for the FSI_SCOM_SET_INTERRUPT here correspond to
    // the complement mask (0x100c) and true mask (0x100d) of fsi2pib.
    si.comp_mask = comp_mask;
    si.true_mask = true_mask;

    rc = ioctl(fd, FSI_SCOM_SET_INTERRUPT, &si);
    if (rc < 0)
        return -errno;

    return 0;
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
    boost::asio::io_context io;

    // One "/dev/scom#" file will exist per hub. Each one needs to be monitored
    // for attentions, so loop through all active hubs.
    pdbg_target* hubTarget;
    pdbg_for_each_class_target("hub", hubTarget)
    {
        // Active hubs only.
        if (PDBG_TARGET_ENABLED !=
            pdbg_target_probe(util::pdbg::getPibTrgt(hubTarget)))
            continue;

        uint32_t fapiPos = std::numeric_limits<uint32_t>::max();
        pdbg_target_get_attribute(hubTarget, "ATTR_FAPI_POS", 4, 1, &fapiPos);

        char scomName[64];
        sprintf(scomName, "/dev/scom%d", fapiPos);
        int fd = open(scomName, O_RDWR);

        if (fd < 0)
        {
            trace::err("failed to get file descriptor");
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

        boost::asio::posix::stream_descriptor sd(io);
        sd.assign(fd);
        scheduleFsiEvent(sd);
    }

    io.run();
}

} // namespace attn
