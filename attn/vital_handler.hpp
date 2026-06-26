#pragma once

#include <attn/attn-event.hpp>

namespace attn
{

/**
 * @brief Handle SBE vital attention
 *
 * @param i_event An attention event.
 * @return 0 indicates that the vital attention was successfully handled
 *         1 indicates that the vital attention was NOT successfully handled
 */
int handleVital(Event* i_event);

} // namespace attn
