#pragma once

#include <attn/attn_config.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/posix/stream_descriptor.hpp>

#include <memory>
#include <vector>

namespace attn
{

/**
 *  @brief Responsible for monitoring attention FSI state change
 */
class FsiAttnMonitor
{
  public:
    FsiAttnMonitor() = delete;
    ~FsiAttnMonitor() = default;

    /** @brief Constructs FsiAttnMonitor object.
     *
     * The FsiAttnMonitor constructor will create a new object and start
     * the objects associated GPIO listener.
     *
     * @param i_attnConfig poiner to attention handler configuration object
     */
    FsiAttnMonitor(Config* i_attnConfig) : iv_config(i_attnConfig)
    {
        configureFsiEvent(); // registers the event handler
    }

    // delete copy constructor
    FsiAttnMonitor(const FsiAttnMonitor&) = delete;

    // delete assignment operator
    FsiAttnMonitor& operator=(const FsiAttnMonitor&) = delete;

    // delete move copy consructor
    FsiAttnMonitor(FsiAttnMonitor&&) = delete;

    // delete move assignment operator
    FsiAttnMonitor& operator=(FsiAttnMonitor&&) = delete;

  private: // instance variables
    /** @brief attention handler configuration object pointer */
    Config* iv_config;

    /** @brief io_context for async operations */
    boost::asio::io_context iv_io;

    /** @brief stream descriptors for monitoring FSI events */
    std::vector<std::unique_ptr<boost::asio::posix::stream_descriptor>>
        iv_streamDescriptors;

  private: // class methods
    /** @brief schedule an FSI event handler */
    void scheduleFsiEvent(boost::asio::posix::stream_descriptor& i_eventDesc);

    /** @brief handle the FSI event */
    void handleFsiEvent(boost::asio::posix::stream_descriptor& i_eventDesc);

    /** @brief Configure an FSI event */
    void configureFsiEvent();
};

/**
 * @brief Configures the complement mask (0x100C) and true mask (0x100D) for
 *         all hub chips.
 */
void configureFsi2Pib();

} // namespace attn
