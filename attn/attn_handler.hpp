#pragma once

#include <attn/attn_config.hpp>

#include <cstdint>

namespace attn
{

/**
 * @brief A bit mask of each relevant attention type handled in the FSI2PIB
 *        status register (same for both hub and compute chips).
 */
enum Fsi2PibAttn_t : uint32_t
{
    // clang-format off

    FSI2PIB_ANY_ATTN     = 0x80000000, //  0
    FSI2PIB_CHIP_CS      = 0x40000000, //  1
    FSI2PIB_SPECIAL      = 0x20000000, //  2
    FSI2PIB_RECOVERABLE  = 0x10000000, //  3
    FSI2PIB_COMPUTE_ATTN = 0x04000000, //  5 - hub only
    FSI2PIB_LOCAL_CS     = 0x02000000, //  6
    FSI2PIB_SBE2FSI_INTR = 0x00400000, //  9 - hub only
    FSI2PIB_SPPE_ATTN    = 0x00000002, // 30 - hub only
    FSI2PIB_SBE_ATTN     = 0x00000001, // 31

    // All attentions defined above. Used for clearing interrupts.
    FSI2PIB_ALL_ATTNS =
        FSI2PIB_ANY_ATTN | FSI2PIB_CHIP_CS | FSI2PIB_SPECIAL |
        FSI2PIB_RECOVERABLE | FSI2PIB_COMPUTE_ATTN | FSI2PIB_LOCAL_CS |
        FSI2PIB_SBE2FSI_INTR | FSI2PIB_SPPE_ATTN | FSI2PIB_SBE_ATTN,

    // clang-format on
};

/**
 * @brief Clear attention interrupts
 *
 * The attention interrupts are sticky and may still be set (MPIPL) even if
 * there are no active attentions. If there is an active attention then
 * clearing the associated interrupt will have no effect.
 */
void clearAttnInterrupts();

/**
 * @brief The main attention handler logic
 *
 * Check each processor for active attentions of type SBE Vital (vital),
 * System Checkstop (checkstop) and Special Attention (special) and handle
 * each as follows:
 *
 * checkstop: Call hardware error analyzer
 * vital:     TBD
 * special:   Determine if the special attention is a Breakpoint (BP),
 *            Terminate Immediately (TI) or CoreCodeToSp (corecode). For each
 *            special attention type, do the following:
 *
 *            BP:          Notify Cronus
 *            TI:          Start host diagnostics mode systemd unit
 *            Corecode:    TBD
 *
 * @param i_config pointer to attention handler configuration object
 */
void attnHandler(Config* i_config);

/**
 * @brief Determine if attention is active and not masked
 *
 * Determine whether an attention needs to be handled and trace details of
 * attention type and whether it is masked or not.
 *
 * @param i_val attention status register
 * @param i_mask attention true mask register
 * @param i_attn attention type
 * @param i_proc processor associated with registers
 *
 * @return true if attention is active and not masked, otherwise false
 */
bool activeAttn(uint32_t i_val, uint32_t i_mask, uint32_t i_attn);

} // namespace attn
