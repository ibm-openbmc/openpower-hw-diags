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

    /**
     * @brief Constructor from components.
     * @param i_priority The event priority.
     * @param i_callback A callback function if this event needs to be handled.
     * @param i_target   The target associated with this event.
     * @param i_config   The current attention handler config.
     */
    Event(Priority_t i_priority, int (*i_callback)(Event*),
          TARGETING::TargetPtr i_target, Config* i_config) :
        iv_priority(i_priority), iv_callback(i_callback), iv_target(i_target),
        iv_config(i_config)
    {}

    /** @brief Destructor. */
    ~Event() = default;

    /** @brief Copy constructor. */
    Event(const Event&) = default;

    /** @brief Assignment operator. */
    Event& operator=(const Event&) = default;

    /** @brief Priority accessor. */
    Priority_t getPriority() const
    {
        return iv_priority;
    }

    /** @brief Target accessor. */
    TARGETING::TargetPtr getTarget() const
    {
        return iv_target;
    }

    /** @brief Config accessor. */
    Config* getConfig() const
    {
        return iv_config;
    }

    /** @brief Initiates the callback function for this attention event. */
    int handle()
    {
        return iv_callback(this);
    }

    /**
     * @brief Less than operator. Used to sort these objects by event priority.
     */
    bool operator<(const Event& right) const
    {
        return (getPriority() < right.getPriority());
    }

  private:
    /** The event priority. */
    Priority_t iv_priority;

    /** A callback function if this event needs to be handled */
    int (*iv_callback)(Event*);

    /** The target associated with this event. */
    TARGETING::TargetPtr iv_target;

    /** The target associated with this event. */
    Config* iv_config;
};

} // namespace attn
