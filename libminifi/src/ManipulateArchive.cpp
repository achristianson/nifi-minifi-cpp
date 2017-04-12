/**
 * @file ManipulateArchive.cpp
 * ManipulateArchive class implementation
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

#include <boost/filesystem.hpp>

#include <archive.h>
#include <archive_entry.h>

#include "ManipulateArchive.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

const std::string ManipulateArchive::ProcessorName("ManipulateArchive");
Property ManipulateArchive::Operation("Operation", "Operation to perform on the archive", "");
Property ManipulateArchive::Target("Target", "The path within the archive to perform the operation on", "");
Property ManipulateArchive::Destination("Destination", "Destination for operations (move or copy) which result in new entries", "");
Property ManipulateArchive::Before("Before", "For operations which result in new entries, places the new entry before the entry specified by this property", "");
Property ManipulateArchive::After("After", "For operations which result in new entries, places the new entry after the entry specified by this property", "");
Relationship ManipulateArchive::Success("success", "success operational on the flow record");

void ManipulateArchive::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(Operation);
	properties.insert(Target);
	properties.insert(Destination);
	properties.insert(Before);
	properties.insert(After);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void ManipulateArchive::onTrigger(ProcessContext *context, ProcessSession *session)
{
	FlowFileRecord *flowFile = session->get();

	if (!flowFile)
	{
		return;
	}

	std::string operation;
	context->getProperty(Operation.getName(), operation);

	std::string targetEntry;
	context->getProperty(Target.getName(), targetEntry);

	std::string destination;
	context->getProperty(Destination.getName(), destination);

	std::string before;
	context->getProperty(Before.getName(), before);

	std::string after;
	context->getProperty(After.getName(), after);

	//TODO Validate properties

	FocusArchiveEntry::ArchiveMetadata archiveMetadata;
	FocusArchiveEntry::ReadCallback readCallback(&archiveMetadata);
	session->read(flowFile, &readCallback);

	_logger->log_info("ManipulateArchive performing operation %s on %s", operation.c_str(), targetEntry.c_str());

	// Perform operation: REMOVE
	if (operation == "remove")
	{
		for (auto it = archiveMetadata.entryMetadata.begin(); it != archiveMetadata.entryMetadata.end();)
		{ 
			if ((*it).entryName == targetEntry)
			{
				_logger->log_info("ManipulateArchive found entry %s for removal", targetEntry.c_str());
				std::remove((*it).tmpFileName.c_str());
				it = archiveMetadata.entryMetadata.erase(it);
				break;
			}
			else
			{
				it++;
			}
		}
	}

	// Perform operation: COPY
	if (operation == "copy")
	{
		bool found = false;
		FocusArchiveEntry::ArchiveEntryMetadata copy;

		// Find item to copy
		for (auto it = archiveMetadata.entryMetadata.begin(); it != archiveMetadata.entryMetadata.end();)
		{ 
			if ((*it).entryName == targetEntry)
			{
				_logger->log_info("ManipulateArchive found entry %s to copy", targetEntry.c_str());
				copy = *it;
				found = true;
				break;
			}
			else
			{
				it++;
			}
		}

		if (found)
		{
			// Copy tmp file
			const auto origTmpFileName = copy.tmpFileName;
			const auto newTmpFileName = boost::filesystem::unique_path().native();
			copy.tmpFileName = newTmpFileName;

			{
				std::ifstream src(origTmpFileName, std::ios::binary);
				std::ofstream dst(newTmpFileName, std::ios::binary);
				dst << src.rdbuf();
			}

			copy.entryName = destination;

			// Update metadata
			if (after != "")
			{
				for (auto it = archiveMetadata.entryMetadata.begin();;)
				{

					if (it == archiveMetadata.entryMetadata.end())
					{
						_logger->log_info("ManipulateArchive could not find entry %s to insert copy after, so copy will be appended to end of archive", after.c_str());
						archiveMetadata.entryMetadata.insert(it, copy);
						break;
					}

					if ((*it).entryName == after)
					{
						_logger->log_info("ManipulateArchive found entry %s to insert copy after", after.c_str());
						it++;
						archiveMetadata.entryMetadata.insert(it, copy);
						break;
					}
					else
					{
						it++;
					}
				}
			}
			else
			{
				for (auto it = archiveMetadata.entryMetadata.begin();;)
				{

					if (it == archiveMetadata.entryMetadata.end())
					{
						_logger->log_info("ManipulateArchive could not find entry %s to insert copy before, so copy will be appended to end of archive", before.c_str());
						archiveMetadata.entryMetadata.insert(it, copy);
						break;
					}

					if ((*it).entryName == before)
					{
						_logger->log_info("ManipulateArchive found entry %s to insert copy before", before.c_str());
						archiveMetadata.entryMetadata.insert(it, copy);

						break;
					}
					else
					{
						it++;
					}
				}
			}
		}
		else
		{
			_logger->log_info("ManipulateArchive could not find entry %s to copy", targetEntry.c_str());
		}
	}

	// Perform operation: MOVE
	if (operation == "move")
	{
		bool found = false;
		FocusArchiveEntry::ArchiveEntryMetadata moveEntry;

		// Find item to move
		for (auto it = archiveMetadata.entryMetadata.begin(); it != archiveMetadata.entryMetadata.end();)
		{ 
			if ((*it).entryName == targetEntry)
			{
				_logger->log_info("ManipulateArchive found entry %s to move", targetEntry.c_str());
				moveEntry = *it;
				it = archiveMetadata.entryMetadata.erase(it);
				found = true;
				break;
			}
			else
			{
				it++;
			}
		}

		if (found)
		{
			moveEntry.entryName = destination;

			// Update metadata
			if (after != "")
			{
				for (auto it = archiveMetadata.entryMetadata.begin();;)
				{

					if (it == archiveMetadata.entryMetadata.end())
					{
						_logger->log_info("ManipulateArchive could not find entry %s to move entry after, so entry will be appended to end of archive", after.c_str());
						archiveMetadata.entryMetadata.insert(it, moveEntry);
						break;
					}

					if ((*it).entryName == after)
					{
						_logger->log_info("ManipulateArchive found entry %s to move entry after", after.c_str());
						it++;
						archiveMetadata.entryMetadata.insert(it, moveEntry);
						break;
					}
					else
					{
						it++;
					}
				}
			}
			else
			{
				for (auto it = archiveMetadata.entryMetadata.begin();;)
				{

					if (it == archiveMetadata.entryMetadata.end())
					{
						_logger->log_info("ManipulateArchive could not find entry %s to move entry before, so entry will be appended to end of archive", before.c_str());
						archiveMetadata.entryMetadata.insert(it, moveEntry);
						break;
					}

					if ((*it).entryName == before)
					{
						_logger->log_info("ManipulateArchive found entry %s to move entry before", before.c_str());
						archiveMetadata.entryMetadata.insert(it, moveEntry);

						break;
					}
					else
					{
						it++;
					}
				}
			}
		}
		else
		{
			_logger->log_info("ManipulateArchive could not find entry %s to move", targetEntry.c_str());
		}
	}

	// Perform operation: TOUCH
	if (operation == "touch")
	{
		FocusArchiveEntry::ArchiveEntryMetadata touchEntry;
		touchEntry.entryName = targetEntry;
		touchEntry.entryType = AE_IFREG;

		// Update metadata
		if (after != "")
		{
			for (auto it = archiveMetadata.entryMetadata.begin();;)
			{

				if (it == archiveMetadata.entryMetadata.end())
				{
					_logger->log_info("ManipulateArchive could not find entry %s to touch entry after, so entry will be appended to end of archive", after.c_str());
					archiveMetadata.entryMetadata.insert(it, touchEntry);
					break;
				}

				if ((*it).entryName == after)
				{
					_logger->log_info("ManipulateArchive found entry %s to touch entry after", after.c_str());
					it++;
					archiveMetadata.entryMetadata.insert(it, touchEntry);
					break;
				}
				else
				{
					it++;
				}
			}
		}
		else
		{
			for (auto it = archiveMetadata.entryMetadata.begin();;)
			{

				if (it == archiveMetadata.entryMetadata.end())
				{
					_logger->log_info("ManipulateArchive could not find entry %s to touch entry before, so entry will be appended to end of archive", before.c_str());
					archiveMetadata.entryMetadata.insert(it, touchEntry);
					break;
				}

				if ((*it).entryName == before)
				{
					_logger->log_info("ManipulateArchive found entry %s to touch entry before", before.c_str());
					archiveMetadata.entryMetadata.insert(it, touchEntry);

					break;
				}
				else
				{
					it++;
				}
			}
		}
	}

	UnfocusArchiveEntry::WriteCallback writeCallback(&archiveMetadata);
	session->write(flowFile, &writeCallback);

	session->transfer(flowFile, Success);
}
