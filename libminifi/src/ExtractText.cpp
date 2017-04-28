/**
 * @file ExtractText.cpp
 * ExtractText class implementation
 *
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <iterator>
#include <string>
#include <string.h>
#include <iostream>
#include <fstream>

#include "ExtractText.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

const std::string ExtractText::ProcessorName("ExtractText");
Property ExtractText::Attribute("Attribute", "Attribute to set from content (TEMPORARY)", "");
Relationship ExtractText::Success("success", "success operational on the flow record");

void ExtractText::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(Attribute);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void ExtractText::onTrigger(ProcessContext *context, ProcessSession *session)
{
	FlowFileRecord *flowFile = session->get();

	if (!flowFile)
	{
		return;
	}

	ReadCallback cb(flowFile, context);
	session->read(flowFile, &cb);
	session->transfer(flowFile, Success);
}

void ExtractText::ReadCallback::process(std::ifstream *stream)
{
	std::string attrKey;
	_ctx->getProperty(Attribute.getName(), attrKey);
	std::string contentStr(std::istreambuf_iterator<char>(*stream), {});
	_flowFile->setAttribute(attrKey, contentStr);
}

ExtractText::ReadCallback::ReadCallback(FlowFileRecord *flowFile, ProcessContext *ctx)
{
	_logger = Logger::getLogger();
	_flowFile = flowFile;
	_ctx = ctx;
}

ExtractText::ReadCallback::~ReadCallback()
{
}