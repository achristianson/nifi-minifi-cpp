/**
 * @file UnfocusArchiveEntry.h
 * UnfocusArchiveEntry class declaration
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
#ifndef __UNFOCUS_ARCHIVE_H__
#define __UNFOCUS_ARCHIVE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

#include "FocusArchiveEntry.h"

//! UnfocusArchiveEntry Class
class UnfocusArchiveEntry : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	UnfocusArchiveEntry(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_logger = Logger::getLogger();
	}
	//! Destructor
	virtual ~UnfocusArchiveEntry()
	{
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	//! Supported Relationships
	static Relationship Success;

	//! OnTrigger method, implemented by NiFi UnfocusArchiveEntry
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi UnfocusArchiveEntry
	virtual void initialize(void);

	typedef FocusArchiveEntry::ArchiveEntryMetadata ArchiveEntryMetadata;
	typedef FocusArchiveEntry::ArchiveMetadata ArchiveMetadata;

	//! Write callback for reconstituting lensed archive into flow file content
	class WriteCallback : public OutputStreamCallback
	{
	public:
		WriteCallback(ArchiveMetadata *archiveMetadata);
		void process(std::ofstream *stream);

	private:
		//! Logger
		std::shared_ptr<Logger> _logger;
		ArchiveMetadata *_archiveMetadata;

	};

protected:

private:
	//! Logger
	std::shared_ptr<Logger> _logger;
};

#endif
