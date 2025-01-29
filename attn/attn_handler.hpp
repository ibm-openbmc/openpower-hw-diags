#pragma once

#include <attn/attn_config.hpp>

#include <cstdint>

namespace attn
{
/** @brief Attention global status bits */
constexpr uint32_t SBE_ATTN = 0x00000002;
constexpr uint32_t ANY_ATTN = 0x80000000;
constexpr uint32_t CHECKSTOP_ATTN = 0x40000000;
constexpr uint32_t SPECIAL_ATTN = 0x20000000;
constexpr uint32_t RECOVERABLE_ATTN = 0x10000000;

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
