#include <attn/attention.hpp>
#include <attn/attn_config.hpp>
#include <attn/attn_handler.hpp>
#include <cli.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

#include <vector>

namespace attn
{
// these are in the attn_lib but not all exposed via headers
int handleSpecial(Attention* i_attention);
int handleCheckstop(Attention* i_attention);
int handleVital(Attention* i_attention);
} // namespace attn

/** @brief Attention handler test application */
int main(int argc, char* argv[])
{
    int rc = 0; // return code

    // initialize phal targets
    TARGETING::utils::targetingInit();

    // create attention handler config object
    attn::Config attnConfig;

    // convert cmd line args to config values
    parseConfig(argv, argv + argc, &attnConfig);

    // exercise attention gpio event path
    attn::attnHandler(&attnConfig);

    // Get a hub for testing
    TARGETING::TargetPtrList hubList =
        TARGETING::utils::getTargets(TARGETING::TYPE_HUB_CHIP);

    // Exercise special, checkstop and vital attention handler paths
    if ((nullptr != hubList[0]) && TARGETING::utils::isFunctional(hubList[0]))
    {
        std::vector<attn::Attention> attentions;

        attentions.emplace_back(attn::Attention::AttentionType::Special,
                                attn::handleSpecial, hubList[0], &attnConfig);

        attentions.emplace_back(attn::Attention::AttentionType::Checkstop,
                                attn::handleCheckstop, hubList[0], &attnConfig);

        attentions.emplace_back(attn::Attention::AttentionType::Vital,
                                attn::handleVital, hubList[0], &attnConfig);

        std::for_each(std::begin(attentions), std::end(attentions),
                      [](attn::Attention attention) {
                          trace::inf("calling handler");
                          attention.handle();
                      });
    }

    return rc;
}
