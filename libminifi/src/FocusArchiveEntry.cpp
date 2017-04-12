/**
 * @file FocusArchiveEntry.cpp
 * FocusArchiveEntry class implementation
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

#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"

#include "FocusArchiveEntry.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

using namespace rapidjson;

const std::string FocusArchiveEntry::ProcessorName("FocusArchiveEntry");
Property FocusArchiveEntry::Path("Path", "The path within the archive to focus (\"/\" to focus the total archive)", "");
Relationship FocusArchiveEntry::Success("success", "success operational on the flow record");

void FocusArchiveEntry::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	properties.insert(Path);
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void FocusArchiveEntry::onTrigger(ProcessContext *context, ProcessSession *session)
{
	FlowFileRecord *flowFile = session->get();

	if (!flowFile)
	{
		return;
	}

	std::string targetEntry;
	context->getProperty(Path.getName(), targetEntry);

	// Extract archive contents
	ArchiveMetadata archiveMetadata;
	archiveMetadata.focusedEntry = targetEntry;
	ReadCallback cb(&archiveMetadata);
	session->read(flowFile, &cb);

	// For each extracted entry, import & stash to key
	std::string targetEntryStashKey;

	for (auto &entryMetadata : archiveMetadata.entryMetadata)
	{
		if (entryMetadata.entryType == AE_IFREG)
		{
			_logger->log_info("FocusArchiveEntry importing %s from %s",
					entryMetadata.entryName.c_str(),
					entryMetadata.tmpFileName.c_str());
			session->import(entryMetadata.tmpFileName, flowFile, false, 0);
			char stashKey[37];
			uuid_t stashKeyUuid;
			uuid_generate(stashKeyUuid);
			uuid_unparse_lower(stashKeyUuid, stashKey);
			_logger->log_debug("FocusArchiveEntry generated stash key %s for entry %s", stashKey, entryMetadata.entryName.c_str());
			entryMetadata.stashKey.assign(stashKey);

			if (entryMetadata.entryName == targetEntry)
			{
				targetEntryStashKey = entryMetadata.stashKey;
			}

			// Stash the content
			session->stash(entryMetadata.stashKey, flowFile);
		}
	}

	// Restore target archive entry
	if (targetEntryStashKey != "")
	{
		session->restore(targetEntryStashKey, flowFile);
	}
	else
	{
		_logger->log_warn("FocusArchiveEntry failed to locate target entry: %s", targetEntry.c_str());
	}

	// Set new/updated lens stack to attribute
	{
		Document doc;
		Document::AllocatorType &alloc = doc.GetAllocator();

		std::string existingLensStack;

		if (flowFile->getAttribute("lens.archive.stack", existingLensStack))
		{
			_logger->log_info("FocusArchiveEntry loading existing lens context");
			doc.Parse(existingLensStack.c_str());
		}
		else
		{
			doc.SetArray();
		}

		Value structVal;
		structVal.SetArray();

		for (const auto &entryMetadata : archiveMetadata.entryMetadata)
		{
			Value entryVal;
			entryVal.SetObject();

			Value entryNameVal;
			entryNameVal.SetString(entryMetadata.entryName.c_str(), entryMetadata.entryName.length());
			entryVal.AddMember("entry_name", entryNameVal, alloc);

			entryVal.AddMember("entry_type", entryMetadata.entryType, alloc);
			entryVal.AddMember("entry_perm", entryMetadata.entryPerm, alloc);

			if (entryMetadata.entryType == AE_IFREG)
			{
				Value stashKeyVal;
				stashKeyVal.SetString(entryMetadata.stashKey.c_str(), entryMetadata.stashKey.length());
				entryVal.AddMember("stash_key", stashKeyVal, alloc);
			}

			structVal.PushBack(entryVal, alloc);
		}

		Value lensVal;
		lensVal.SetObject();
		Value formatNameVal;
		formatNameVal.SetString(archiveMetadata.archiveFormatName.c_str(), archiveMetadata.archiveFormatName.length());
		lensVal.AddMember("archive_format_name", formatNameVal, alloc);
		Value focusedEntryVal;
		focusedEntryVal.SetString(archiveMetadata.focusedEntry.c_str(), archiveMetadata.focusedEntry.length());
		lensVal.AddMember("focused_entry", focusedEntryVal, alloc);
		lensVal.AddMember("archive_format", archiveMetadata.archiveFormat, alloc);
		lensVal.AddMember("archive_structure", structVal, alloc);
		doc.PushBack(lensVal, alloc);

		StringBuffer buffer;
		Writer<StringBuffer> writer(buffer);
		doc.Accept(writer);

		auto stackStr = buffer.GetString();

		if (!flowFile->updateAttribute("lens.archive.stack", stackStr))
		{
			flowFile->addAttribute("lens.archive.stack", stackStr);
		}
	}

    // Transfer to the relationship
    session->transfer(flowFile, Success);
}

typedef struct
{
	std::ifstream *stream;
	char buf[8196];
} FocusArchiveEntryReadData;

void FocusArchiveEntry::ReadCallback::process(std::ifstream *stream)
{
	auto inputArchive = archive_read_new();
	struct archive_entry *entry;

	FocusArchiveEntryReadData data;
	data.stream = stream;

	archive_read_support_format_all(inputArchive);
	archive_read_support_filter_all(inputArchive);

	// Read callback which reads from ifstream
	auto read = [] (archive *, void *d, const void **buf) -> long
	{
		auto data = static_cast<FocusArchiveEntryReadData *>(d);
		*buf = data->buf;
		long read = 0;

		while (!data->stream->eof() && read < 8196)
		{
			data->stream->read(data->buf, 8196 - read);
			read += data->stream->gcount();
		}

		return read;
	};

	// Close callback for libarchive
	auto close = [] (archive *, void *) -> int
	{
		// Because we do not need to close the stream, do nothing & return success
		return 0;
	};

	// Read each item in the archive
	int res;

	if ((res = archive_read_open(inputArchive, &data, NULL, read, close)))
	{
			_logger->log_error("FocusArchiveEntry can't open due to archive error: %s", archive_error_string(inputArchive));
			return;
	}

	for (;;)
	{
		res = archive_read_next_header(inputArchive, &entry);

		if (res == ARCHIVE_EOF)
		{
			break;
		}

		if (res < ARCHIVE_OK)
		{
			_logger->log_error("FocusArchiveEntry can't read header due to archive error: %s", archive_error_string(inputArchive));
			return;
		}

		if (res < ARCHIVE_WARN)
		{
			_logger->log_warn("FocusArchiveEntry got archive warning while reading header: %s", archive_error_string(inputArchive));
			return;
		}

		auto entryName = archive_entry_pathname(entry);
		(*_archiveMetadata).archiveFormatName.assign(archive_format_name(inputArchive));
		(*_archiveMetadata).archiveFormat = archive_format(inputArchive);

		// Record entry metadata
		auto entryType = archive_entry_filetype(entry);

		ArchiveEntryMetadata metadata;
		metadata.entryName = entryName;
		metadata.entryType = entryType;
		metadata.entryPerm = archive_entry_perm(entry);

		_logger->log_info("FocusArchiveEntry entry type of %s is: %d", entryName, metadata.entryType);
		_logger->log_info("FocusArchiveEntry entry perm of %s is: %d", entryName, metadata.entryPerm);

		// Write content to tmp file
		if (entryType == AE_IFREG)
		{
			auto tmpFileName = boost::filesystem::unique_path().native();
			metadata.tmpFileName = tmpFileName;
			metadata.entryType = entryType;
			_logger->log_info("FocusArchiveEntry extracting %s to: %s", entryName, tmpFileName.c_str());

			auto fd = fopen(tmpFileName.c_str(), "w");

			if (archive_entry_size(entry) > 0)
			{
				archive_read_data_into_fd(inputArchive, fileno(fd));
			}

			fclose(fd);
		}

		(*_archiveMetadata).entryMetadata.push_back(metadata);
	}

	archive_read_close(inputArchive);
	archive_read_free(inputArchive);

}

FocusArchiveEntry::ReadCallback::ReadCallback(ArchiveMetadata *archiveMetadata)
{
	_logger = Logger::getLogger();
	_archiveMetadata = archiveMetadata;
}

FocusArchiveEntry::ReadCallback::~ReadCallback()
{
}
