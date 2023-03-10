#include <attn/attn_config.hpp>

namespace attn
{

/** @brief Main constructor */
Config::Config()
{
    setFlagAll();
    iv_flags.reset(dfltTi); // default value is clear
}

/** @brief Get state of flag */
bool Config::getFlag(AttentionFlag i_flag) const
{
    return (iv_flags.test(i_flag));
}

/** @brief Set configuration flag */
void Config::setFlag(AttentionFlag i_flag)
{
    iv_flags.set(i_flag);
}

/** @brief Set all configuration flags */
void Config::setFlagAll()
{
    iv_flags.set(enVital);
    iv_flags.set(enCheckstop);
    iv_flags.set(enTerminate);
    iv_flags.set(enBreakpoints);
    iv_flags.set(enClrAttnIntr);
}

/** @brief Clear configuration flag */
void Config::clearFlag(AttentionFlag i_flag)
{
    iv_flags.reset(i_flag);
}

/** @brief Clear all configuration flags */
void Config::clearFlagAll()
{
    iv_flags.reset(enVital);
    iv_flags.reset(enCheckstop);
    iv_flags.reset(enTerminate);
    iv_flags.reset(enBreakpoints);
    iv_flags.reset(enClrAttnIntr);
}

} // namespace attn
