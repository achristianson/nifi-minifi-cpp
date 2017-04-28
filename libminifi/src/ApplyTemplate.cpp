/**
 * @file ApplyTemplate.cpp
 * ApplyTemplate class implementation
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
#include <string.h>
#include <iostream>
#include <fstream>

#include <boost/iostreams/device/mapped_file.hpp>

#include <bustache/model.hpp>
#include <bustache/format.hpp>

#include "ApplyTemplate.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

const std::string ApplyTemplate::ProcessorName("ApplyTemplate");
Property ApplyTemplate::Template("Template", "Path to the input mustache template file", "");
Relationship ApplyTemplate::Success("success", "success operational on the flow record");

void ApplyTemplate::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(Template);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void ApplyTemplate::onTrigger(ProcessContext *context, ProcessSession *session)
{
	FlowFileRecord *flowFile = session->get();

	if (!flowFile)
	{
		return;
	}

	WriteCallback cb(context, flowFile);
	session->write(flowFile, &cb);
	session->transfer(flowFile, Success);
}

ApplyTemplate::WriteCallback::WriteCallback(ProcessContext *context, FlowFileRecord *flowFile)
{
	_logger = Logger::getLogger();
	_ctx = context;
	_flowFile = flowFile;
}

void ApplyTemplate::WriteCallback::process(std::ofstream *stream)
{
	std::string templateFile;
	_ctx->getProperty(Template.getName(), templateFile);

	boost::iostreams::mapped_file_source file(templateFile);

	bustache::format format(file);
	bustache::object data;

	for (const auto &attr : _flowFile->getAttributes())
	{
		data[attr.first] = attr.second;
	}

	*stream << format(data);
}
