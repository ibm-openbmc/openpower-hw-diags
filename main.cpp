#include <analyzer/analyzer_main.hpp>
#include <attn/attention.hpp>
#include <attn/attn_config.hpp>
#include <attn/attn_dump.hpp>
#include <attn/attn_handler.hpp>
#include <attn/attn_main.hpp>
#include <attn/fsi_attn_monitor.hpp> // for configureFsi2Pib()
#include <buildinfo.hpp>
#include <cli.hpp>
#include <hei_buildinfo.hpp>
#include <util/pdbg.hpp> // for PHAL targeting

/**
 * @brief Attention handler application main()
 *
 * This is the main interface to the hardware diagnostics application. This
 * application can be loaded as a daemon for monitoring the attention
 * gpio or it can be loaded as an application to analyze hardware and
 * diagnose hardware error conditions.
 *
 *     Usage:
 *        --analyze:              Analyze the hardware
 *        --daemon:               Start the attention handler daemon
 *
 * @return 0 = success
 */
int main(int argc, char* argv[])
{
    int rc = 0; // assume success

    if (argc == 1)
    {
        printf("openpower-hw-diags <options>\n");
        printf("options:\n");
        printf("  --analyze:              Analyze the hardware\n");
        printf("  --daemon:               Start the attn handler daemon\n");
        printf("hwdiag: %s, hei: %s\n", BUILDINFO, libhei::getBuildInfo());
    }
    else
    {
        // Pdbg targets should only be initialized once according to
        // libpdbg documentation. Initializing them here will make sure
        // they are initialized for the attention handler, invocation of
        // the analyzer via attention handler and direct invocation of
        // the analyzer via command line (--analyze).
        TARGETING::utils::targetingInit();

        // Either analyze (application mode) or daemon mode
        if (true == getCliOption(argv, argv + argc, "--analyze"))
        {
            // Analyze the host hardware.
            // TODO: At the moment, we'll only do MANUAL analysis (no service
            //       actions). It may be possible in the future to allow command
            //       line options to change the analysis type, if needed.

            attn::DumpParameters dumpParameters;
            analyzer::analyzeHardware(analyzer::AnalysisType::MANUAL,
                                      dumpParameters);
        }
        // daemon mode
        else
        {
            if (true == getCliOption(argv, argv + argc, "--daemon"))
            {
                /* TODO - reenable
                attn::Config attnConfig; // default config

                // convert remaining cmd line args to config values
                parseConfig(argv, argv + argc, &attnConfig);

                attn::configureFsi2Pib(); // Configure fsi2pib true mask

                attn::attnHandler(&attnConfig); // handle pending attentions

                attn::attnDaemon(&attnConfig);  // start daemon'
                */
            }
        }
    }

    return rc;
}
