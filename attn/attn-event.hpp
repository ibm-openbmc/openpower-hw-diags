#pragma once

#include <attn/attn_config.hpp>
#include <util/pdbg.hpp>

namespace attn
{

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
    /** @brief Supported events and their priorities. */
    enum Priority_t
    {
        // While checkstop attentions are most critical, the SPPE is required to
        // do any SCOMs for analysis or initiate dumps. Therefore, this is the
        // highest priority.
        PRI_SPPE_ATTN = 2,

        // Critial error. The host is dead. Higher than special attentions.
        PRI_CHECKSTOP = 1,

        // For TIs and breakpoints. Lowest priority
        PRI_SPECIAL = 0,
    };

    /** @brief Default constructor. */
    Event() = delete;

    /** @brief Main constructor. */
    Event(Priority_t i_priority, int (*i_handler)(Event*),
          TARGETING::TargetPtr i_target, Config* i_config) :
        iv_priority(i_priority), iv_handler(i_handler), iv_target(i_target),
        iv_config(i_config)
    {}

    /** @brief Destructor */
    ~Event() = default;

    /** @brief Copy constructor. */
    Event(const Event&) = default;

    /** @brief Assignment operator. */
    Event& operator=(const Event&) = default;

    /** @brief Get attention priority */
    Priority_t getPriority() const
    {
        return iv_priority;
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
    Priority_t iv_priority;         // The event priority.
    int (*iv_handler)(Event*);      // handler function
    TARGETING::TargetPtr iv_target; // handler function target
    Config* iv_config;              // configuration flags
};

} // namespace attn
