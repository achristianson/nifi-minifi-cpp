/**
 * @file ManipulateArchive.h
 * ManipulateArchive class declaration
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
#ifndef __MANIPULATE_ARCHIVE_H__
#define __MANIPULATE_ARCHIVE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

#include "FocusArchiveEntry.h"
#include "UnfocusArchiveEntry.h"

//! ManipulateArchive Class
class ManipulateArchive : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	ManipulateArchive(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_logger = Logger::getLogger();
	}
	//! Destructor
	virtual ~ManipulateArchive()
	{
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property Operation;
	static Property Target;
	static Property Destination;
	static Property Before;
	static Property After;
	//! Supported Relationships
	static Relationship Success;

	//! OnTrigger method, implemented by NiFi ManipulateArchive
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi ManipulateArchive
	virtual void initialize(void);

protected:

private:
	//! Logger
	std::shared_ptr<Logger> _logger;
};

#endif
