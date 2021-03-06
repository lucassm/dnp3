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

#include "Action0.h"

#include <cstring>

namespace openpal
{

Action0::Action0() : Erasure(), pInvoke(nullptr)
{}

Action0::Action0(Invoke pInvoke_, uint32_t size_) : Erasure(size_), pInvoke(pInvoke_)
{}

bool Action0::IsSet() const
{
	return (pInvoke != nullptr);
}

Action0& Action0::operator=(const Action0& other)
{
	if (this != &other)
	{		
		this->pInvoke = other.pInvoke;
		this->CopyErasure(other);
	}

	return (*this);
}

Action0::Action0(const Action0& other) : Erasure(), pInvoke(other.pInvoke)
{
	this->CopyErasure(other);
}

void Action0::Apply() const
{
	if (pInvoke)
	{
		(*pInvoke)(bytes);
	}
}

}
