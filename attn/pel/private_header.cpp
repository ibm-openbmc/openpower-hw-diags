#include "private_header.hpp"

namespace attn
{
namespace pel
{

PrivateHeader::PrivateHeader(Stream& pel)
{
    unflatten(pel);
}

void PrivateHeader::flatten(Stream& stream) const
{
    stream << _header << _createTimestamp << _commitTimestamp << _creatorID
           << _reservedByte1 << _reservedByte2 << _sectionCount << _obmcLogID
           << _creatorVersion << _plid << _id;
}

void PrivateHeader::unflatten(Stream& stream)
{
    stream >> _header >> _createTimestamp >> _commitTimestamp >> _creatorID >>
        _reservedByte1 >> _reservedByte2 >> _sectionCount >> _obmcLogID >>
        _creatorVersion >> _plid >> _id;
}

uint8_t PrivateHeader::getSectionCount()
{
    return _sectionCount;
}

void PrivateHeader::setSectionCount(uint8_t sectionCount)
{
    _sectionCount = sectionCount;
}

void PrivateHeader::setPlid(uint32_t plid)
{
    _plid = plid;
}

} // namespace pel
} // namespace attn
