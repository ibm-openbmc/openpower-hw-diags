//------------------------------------------------------------------------------
// IMPORTANT:
// This file will NOT be built in CI test and should be used for any functions
// that require addition support to simulate in CI test. Any functions that will
// work out-of-the-box in CI test with use of the fake device tree should be put
// in `pdbg.cpp`.
//------------------------------------------------------------------------------

#include <assert.h>

#include <util/log.hpp>
#include <util/pdbg.hpp>
#include <util/trace.hpp>

using namespace analyzer;

namespace util
{

namespace pdbg
{

//------------------------------------------------------------------------------

bool queryLpcTimeout(TARGETING::TargetPtr i_target)
{
    // Must be a hub target.
    assert(TARGETING::TYPE_HUB_CHIP == getTrgtType(i_target));

    uint32_t result = 0;
    /* TODO - updated interface?
    if (0 != sbe_lpc_timeout(util::pdbg::getPibTrgt(i_target), &result))
    {
        trace::err("sbe_lpc_timeout() failed: i_target=%s", getPath(i_target));
        result = 0; // just in case
    }*/

    // 0 if no timeout, 1 if LPC timeout occurred.
    return (0 != result);
}

//------------------------------------------------------------------------------

int getScom(TARGETING::TargetPtr i_target, uint64_t i_addr, uint64_t& o_val)
{
    assert(nullptr != i_target);

    int rc = 0;

    try
    {
        rc = hwaccess::HwAccessIntf::getScomRegister(i_target, i_addr, o_val);
        if (0 != rc)
        {
            lg2::error(
                "SCOM read failure: target={SCOM_TARGET} addr={SCOM_ADDRESS}",
                "SCOM_TARGET", getPath(i_target), "SCOM_ADDRESS",
                (lg2::hex | lg2::field64), i_addr, "SCOM_ACCESS_RC", rc);
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "SCOM read exception: target={SCOM_TARGET} addr={SCOM_ADDRESS}",
            "SCOM_TARGET", getPath(i_target), "SCOM_ADDRESS",
            (lg2::hex | lg2::field64), i_addr);
        lg2::error(e.what());
        return -1;
    }

    return rc;
}

//------------------------------------------------------------------------------

int getCfam(TARGETING::TargetPtr i_target, uint32_t i_addr, uint32_t& o_val)
{
    assert(nullptr != i_target);
    assert(TARGETING::TYPE_HUB_CHIP == getTrgtType(i_target));

    int rc = 0;

    try
    {
        rc = hwaccess::HwAccessIntf::getCfamRegister(i_target, i_addr, o_val);
        if (0 != rc)
        {
            lg2::error(
                "CFAM read failure: target={CFAM_TARGET} addr={CFAM_ADDRESS}",
                "CFAM_TARGET", getPath(i_target), "CFAM_ADDRESS",
                (lg2::hex | lg2::field32), i_addr, "CFAM_ACCESS_RC", rc);
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "CFAM read exception: target={CFAM_TARGET} addr={CFAM_ADDRESS}",
            "CFAM_TARGET", getPath(i_target), "CFAM_ADDRESS",
            (lg2::hex | lg2::field32), i_addr);
        lg2::error(e.what());
        return -1;
    }

    return rc;
}

//------------------------------------------------------------------------------

int putCfam(TARGETING::TargetPtr i_target, uint32_t i_addr, uint32_t i_val)
{
    assert(nullptr != i_target);
    assert(TARGETING::TYPE_HUB_CHIP == getTrgtType(i_target));

    int rc = 0;

    try
    {
        rc = hwaccess::HwAccessIntf::putCfamRegister(i_target, i_addr, i_val);
        if (0 != rc)
        {
            lg2::error(
                "CFAM write failure: target={CFAM_TARGET} addr={CFAM_ADDRESS}",
                "CFAM_TARGET", getPath(i_target), "CFAM_ADDRESS",
                (lg2::hex | lg2::field32), i_addr, "CFAM_ACCESS_RC", rc);
        }
    }
    catch (const std::exception& e)
    {
        lg2::error(
            "CFAM write exception: target={CFAM_TARGET} addr={CFAM_ADDRESS}",
            "CFAM_TARGET", getPath(i_target), "CFAM_ADDRESS",
            (lg2::hex | lg2::field32), i_addr);
        lg2::error(e.what());
        return -1;
    }

    return rc;
}

//------------------------------------------------------------------------------

} // namespace pdbg

} // namespace util
