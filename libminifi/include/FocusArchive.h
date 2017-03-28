/**
 * @file FocusArchive.h
 * FocusArchive class declaration
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
#ifndef __FOCUS_ARCHIVE_H__
#define __FOCUS_ARCHIVE_H__

#include "FlowFileRecord.h"
#include "Processor.h"
#include "ProcessSession.h"

//! FocusArchive Class
class FocusArchive : public Processor
{
public:
	//! Constructor
	/*!
	 * Create a new processor
	 */
	FocusArchive(std::string name, uuid_t uuid = NULL)
	: Processor(name, uuid)
	{
		_logger = Logger::getLogger();
	}
	//! Destructor
	virtual ~FocusArchive()
	{
	}
	//! Processor Name
	static const std::string ProcessorName;
	//! Supported Properties
	static Property Path;
	//! Supported Relationships
	static Relationship Success;

	//! OnTrigger method, implemented by NiFi FocusArchive
	virtual void onTrigger(ProcessContext *context, ProcessSession *session);
	//! Initialize, over write by NiFi FocusArchive
	virtual void initialize(void);

	typedef struct
	{
		std::string entryName;
		std::string tmpFileName;
		std::string stashKey;
		mode_t entryType;
		mode_t entryPerm;
	} ArchiveEntryMetadata;

	typedef struct
	{
		std::string archiveType;
		int archiveTypeId;
		std::map<std::string, ArchiveEntryMetadata> entryMetadata;
	} ArchiveMetadata;

	class ReadCallback : public InputStreamCallback
	{
	public:
		ReadCallback(ProcessSession *session, ArchiveMetadata *archiveMetadata);
		~ReadCallback();
		virtual void process(std::ifstream *stream);

	private:
		std::shared_ptr<Logger> _logger;
		ProcessSession *_session;
		ArchiveMetadata *_archiveMetadata;
	};

protected:

private:
	//! Logger
	std::shared_ptr<Logger> _logger;
};

#endif
