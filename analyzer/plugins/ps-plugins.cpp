#include <analyzer/plugins/plugin.hpp>

namespace analyzer
{

namespace PS
{

void omi_degrade(unsigned int, const libhei::Chip&, ServiceData&)
{
    // TODO
}

//------------------------------------------------------------------------------

void channel_timeout(unsigned int, const libhei::Chip&, ServiceData&)
{
    // TODO
}

} // namespace PS

PLUGIN_DEFINE_NS(PS_10, PS, omi_degrade);
PLUGIN_DEFINE_NS(PS_20, PS, omi_degrade);

PLUGIN_DEFINE_NS(PS_10, PS, channel_timeout);
PLUGIN_DEFINE_NS(PS_20, PS, channel_timeout);

} // namespace analyzer
