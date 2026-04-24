#include <analyzer/plugins/plugin.hpp>

namespace analyzer
{

namespace PT
{

void pb_token_manager(unsigned int, const libhei::Chip&, ServiceData&)
{
    // TODO
}

} // namespace PT

PLUGIN_DEFINE_NS(PT_10, PT, pb_token_manager);

} // namespace analyzer
