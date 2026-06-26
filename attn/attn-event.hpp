#pragma once

#include <attn/attn_config.hpp>
#include <util/pdbg.hpp>

#include <bitset>

namespace attn
{

/** @brief attention handler configuration flags */
inline constexpr uint32_t enableBreakpoints = 1;

/**
 * @brief Contain information about an active attention event.
 *
 * These objects are created for each active attention, which carry with them
 * various configuration and status information as well a callback function for
 * handling the attention. Each object also carries a priority value. This
 * priority is used to determine which attention event(s) to handle when there
 * are more than one active attentions at the same time.
 */
class Event
{
  public:
    /** @brief types of attentions to be handled (by priority low to high) */
    enum AttentionType
    {
        Special = 0,
        Checkstop = 1,
        Vital = 2
    };

    /** @brief Default constructor. */
    Event() = delete;

    /** @brief Main constructor. */
    Event(AttentionType i_type, int (*i_handler)(Event*),
          TARGETING::TargetPtr i_target, Config* i_config) :
        iv_type(i_type), iv_handler(i_handler), iv_target(i_target),
        iv_config(i_config)
    {}

    /** @brief Destructor */
    ~Event() = default;

    /** @brief Copy constructor. */
    Event(const Event&) = default;

    /** @brief Assignment operator. */
    Event& operator=(const Event&) = default;

    /** @brief Get attention priority */
    int getPriority() const
    {
        return iv_type;
    }

    /** @brief Get config object */
    Config* getConfig() const
    {
        return iv_config;
    }

    /** @brief Call attention handler function */
    int handle()
    {
        return iv_handler(this);
    }

    /** @brief Get attention handler target */
    TARGETING::TargetPtr getTarget() const
    {
        return iv_target;
    }

    /** @brief less than operator, for heap creation */
    bool operator<(const Event& right) const
    {
        return (getPriority() < right.getPriority());
    }

  private:
    AttentionType iv_type;          // attention type
    int (*iv_handler)(Event*);      // handler function
    TARGETING::TargetPtr iv_target; // handler function target
    Config* iv_config;              // configuration flags
};

} // namespace attn
