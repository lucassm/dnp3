/**
 * Licensed to Green Energy Corp (www.greenenergycorp.com) under one or
 * more contributor license agreements. See the NOTICE file distributed
 * with this work for additional information regarding copyright ownership.
 * Green Energy Corp licenses this file to you under the Apache License,
 * Version 2.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This project was forked on 01/01/2013 by Automatak, LLC and modifications
 * may have been made to this file. Automatak, LLC licenses these modifications
 * to you under the terms of the License.
 */

#include "APDUParser.h"

#include "opendnp3/gen/QualifierCode.h"
#include "opendnp3/LogLevels.h"
#include "opendnp3/app/GroupVariationRecord.h"
#include "opendnp3/app/MeasurementFactory.h"
#include "opendnp3/app/ObjectHeaderParser.h"

using namespace openpal;

namespace opendnp3
{

ParseResult APDUParser::ParseTwoPass(const openpal::ReadBufferView& buffer, IAPDUHandler* pHandler, openpal::Logger* pLogger, ParserSettings settings)
{
	if(pHandler)
	{
		// do a single pass without the callbacks to validate that the message is well formed
		auto result = ParseSinglePass(buffer, pLogger, nullptr, settings);
		if (result == ParseResult::OK)
		{
			return ParseSinglePass(buffer, nullptr, pHandler, settings);
		}
		else
		{
			return result;
		}		
	}
	else
	{
		return ParseSinglePass(buffer, pLogger, pHandler, settings);
	}
}

ParseResult APDUParser::ParseSinglePass(const openpal::ReadBufferView& buffer, openpal::Logger* pLogger, IAPDUHandler* pHandler, const ParserSettings& settings)
{
	ReadBufferView copy(buffer);
	while(copy.Size() > 0)
	{
		auto result = ParseHeader(copy, pLogger, settings, pHandler);
		if (result != ParseResult::OK)
		{
			return result;
		}
	}
	return ParseResult::OK;
}

ParseResult APDUParser::ParseHeader(ReadBufferView& buffer, openpal::Logger* pLogger, const ParserSettings& settings, IAPDUHandler* pHandler)
{
	ObjectHeader header;
	auto result = ObjectHeaderParser::ParseObjectHeader(header, buffer, pLogger);
	if (result == ParseResult::OK)
	{
		auto gv = GroupVariationRecord::GetRecord(header.group, header.variation);
		if (gv.enumeration == GroupVariation::UNKNOWN)
		{
			FORMAT_LOGGER_BLOCK_WITH_CODE(pLogger, flags::WARN, ALERR_UNKNOWN_GROUP_VAR, "Unknown object %i / %i", gv.group, gv.variation);
			return ParseResult::UNKNOWN_OBJECT;
		}
		else
		{						
			HeaderRecord record(gv, QualifierCodeFromType(header.qualifier));

			switch (record.qualifier)
			{

			case(QualifierCode::ALL_OBJECTS) :
			{
				FORMAT_LOGGER_BLOCK(pLogger, settings.Filters(),
					"%03u,%03u - %s - %s",
					header.group,
					header.variation,
					GroupVariationToString(gv.enumeration),
					QualifierCodeToString(record.qualifier));

				if (pHandler)
				{
					pHandler->AllObjects(record);
				}
		
				return ParseResult::OK;
			}

			case(QualifierCode::UINT8_CNT) :
				return ParseCountHeader<UInt8>(buffer, pLogger, settings, record, pHandler);

			case(QualifierCode::UINT16_CNT) :
				return ParseCountHeader<UInt16>(buffer, pLogger, settings, record, pHandler);

			case(QualifierCode::UINT8_START_STOP) :
				return ParseRangeHeader<UInt8, uint16_t>(buffer, pLogger, settings, record, pHandler);

			case(QualifierCode::UINT16_START_STOP) :
				return ParseRangeHeader<UInt16, uint32_t>(buffer, pLogger, settings, record, pHandler);

			case(QualifierCode::UINT8_CNT_UINT8_INDEX) :
				return ParseIndexPrefixHeader<UInt8>(buffer, pLogger, settings, record, pHandler);

			case(QualifierCode::UINT16_CNT_UINT16_INDEX) :
				return ParseIndexPrefixHeader<UInt16>(buffer, pLogger, settings, record, pHandler);

			default:
				FORMAT_LOGGER_BLOCK_WITH_CODE(pLogger, flags::WARN, ALERR_UNKNOWN_QUALIFIER, "Unknown qualifier %x", header.qualifier);
				return ParseResult::UNKNOWN_QUALIFIER;
			}
		}
	}	
	else
	{
		return result;
	}
}

IndexedValue<Binary, uint16_t> APDUParser::BoolToBinary(const IndexedValue<bool, uint16_t>& v)
{
	return IndexedValue<Binary, uint16_t>(Binary(v.value), v.index);
}

IndexedValue<BinaryOutputStatus, uint16_t> APDUParser::BoolToBinaryOutputStatus(const IndexedValue<bool, uint16_t>& v)
{
	return IndexedValue<BinaryOutputStatus, uint16_t>(BinaryOutputStatus(v.value), v.index);
}

#define MACRO_PARSE_OBJECTS_WITH_RANGE(descriptor) \
	case(GroupVariation::descriptor): \
	return ParseRangeFixedSize(record, descriptor::Inst(), buffer, pLogger, range, pHandler);

ParseResult APDUParser::ParseRangeOfObjects(openpal::ReadBufferView& buffer, openpal::Logger* pLogger, const HeaderRecord& record, const Range& range, IAPDUHandler* pHandler)
{
	switch(record.enumeration)
	{
	case(GroupVariation::Group1Var1):
		return ParseRangeAsBitField(buffer, pLogger, record, range, [pHandler, record](const IterableBuffer<IndexedValue<bool, uint16_t>>& values)
		{
			if(pHandler)
			{
				auto mapped = MapIterableBuffer<IndexedValue<bool, uint16_t>, IndexedValue<Binary, uint16_t>>(&values,
				              [](const IndexedValue<bool, uint16_t>& v)
				{
					return IndexedValue<Binary, uint16_t>(Binary(v.value), v.index);
				}
				                                                                                             );
				pHandler->OnRange(record, mapped);
			}
		});

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group1Var2);

	case(GroupVariation::Group3Var1) :
		return ParseRangeAsDoubleBitField(buffer, pLogger, record, range, [pHandler, record](const IterableBuffer<IndexedValue<DoubleBit, uint16_t>>& values)
		{
			if (pHandler)
			{
				auto mapped = MapIterableBuffer<IndexedValue<DoubleBit, uint16_t>, IndexedValue<DoubleBitBinary, uint16_t>>(&values,
				              [](const IndexedValue<DoubleBit, uint16_t>& v)
				{
					return IndexedValue<DoubleBitBinary, uint16_t>(DoubleBitBinary(v.value), v.index);
				}
				                                                                                                           );
				pHandler->OnRange(record, mapped);
			}
		});

	case(GroupVariation::Group10Var1):
		return ParseRangeAsBitField(buffer, pLogger, record, range, [pHandler, record](IterableBuffer<IndexedValue<bool, uint16_t>>& values)
		{
			if(pHandler)
			{
				auto mapped = MapIterableBuffer<IndexedValue<bool, uint16_t>, IndexedValue<BinaryOutputStatus, uint16_t>>(&values,
				              [](const IndexedValue<bool, uint16_t>& v)
				{
					return IndexedValue<BinaryOutputStatus, uint16_t>(BinaryOutputStatus(v.value), v.index);
				}
				                                                                                                         );
				pHandler->OnRange(record, mapped);
			}
		});

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group3Var2);

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group10Var2);

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group20Var1);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group20Var2);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group20Var5);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group20Var6);

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group21Var1);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group21Var2);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group21Var5);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group21Var6);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group21Var9);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group21Var10);

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group30Var1);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group30Var2);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group30Var3);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group30Var4);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group30Var5);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group30Var6);

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group40Var1);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group40Var2);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group40Var3);
		MACRO_PARSE_OBJECTS_WITH_RANGE(Group40Var4);

		MACRO_PARSE_OBJECTS_WITH_RANGE(Group50Var4);

	case(GroupVariation::Group80Var1):
		return ParseRangeAsBitField(buffer, pLogger, record, range, [pHandler, record](const IterableBuffer<IndexedValue<bool, uint16_t>>& values)
		{
			if(pHandler)
			{
				pHandler->OnIIN(record, values);
			}
		});

	case(GroupVariation::Group110AnyVar):
		return ParseRangeOfOctetData(buffer, pLogger, record, range, pHandler);

	default:
		FORMAT_LOGGER_BLOCK_WITH_CODE(pLogger, flags::WARN, ALERR_ILLEGAL_QUALIFIER_AND_OBJECT,
			"Unsupported qualifier/object - %s - %i / %i",
			QualifierCodeToString(record.qualifier), record.group, record.variation);

		return ParseResult::INVALID_OBJECT_QUALIFIER;
	}
}

ParseResult APDUParser::ParseCountOfObjects(openpal::ReadBufferView& buffer, openpal::Logger* pLogger, const HeaderRecord& record, uint16_t count, IAPDUHandler* pHandler)
{
	switch(record.enumeration)
	{
		case(GroupVariation::Group50Var1) :
			return ParseCountOf<Group50Var1>(buffer, pLogger, record, count, pHandler);

		case(GroupVariation::Group51Var1) :
			return ParseCountOf<Group51Var1>(buffer, pLogger, record, count, pHandler);

		case(GroupVariation::Group51Var2) :
			return ParseCountOf<Group51Var2>(buffer, pLogger, record, count, pHandler);

		case(GroupVariation::Group52Var2) :
			return ParseCountOf<Group52Var2>(buffer, pLogger, record, count, pHandler);
		default:
			return ParseResult::INVALID_OBJECT_QUALIFIER;
	}		
}

ParseResult APDUParser::ParseRangeOfOctetData(
    openpal::ReadBufferView& buffer,
    openpal::Logger* pLogger,
    const HeaderRecord& record,
    const Range& range,
    IAPDUHandler* pHandler)
{
	if (record.variation > 0)
	{
		uint32_t size = record.variation * range.Count();
		if (buffer.Size() < size)
		{
			SIMPLE_LOGGER_BLOCK_WITH_CODE(pLogger, flags::WARN, ALERR_INSUFFICIENT_DATA_FOR_OBJECTS, "Not enough data for specified octet objects");
			return ParseResult::NOT_ENOUGH_DATA_FOR_OBJECTS;
		}
		else
		{
			if (pHandler)
			{
				auto collection = CreateLazyIterable<IndexedValue<OctetString, uint16_t>>(buffer, range.Count(), [record, range](ReadBufferView & buff, uint32_t pos)
				{
					OctetString octets(buff.Take(record.variation));
					IndexedValue<OctetString, uint16_t> value(octets, range.start + pos);
					buff.Advance(record.variation);
					return value;
				});
				pHandler->OnRange(record, collection);
			}
			buffer.Advance(size);
			return ParseResult::OK;
		}
	}
	else
	{
		SIMPLE_LOGGER_BLOCK(pLogger, flags::WARN, "Octet string variation 0 may only be used in requests");
		return ParseResult::INVALID_OBJECT;
	}

}

}

