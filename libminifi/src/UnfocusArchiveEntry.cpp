/**
 * @file UnfocusArchiveEntry.cpp
 * UnfocusArchiveEntry class implementation
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
#include "rapidjson/reader.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/error/en.h"

#include "UnfocusArchiveEntry.h"
#include "ProcessContext.h"
#include "ProcessSession.h"

using namespace rapidjson;

const std::string UnfocusArchiveEntry::ProcessorName("UnfocusArchiveEntry");
Relationship UnfocusArchiveEntry::Success("success", "success operational on the flow record");

void UnfocusArchiveEntry::initialize()
{
	//! Set the supported properties
	std::set<Property> properties;
	setSupportedProperties(properties);
	//! Set the supported relationships
	std::set<Relationship> relationships;
	relationships.insert(Success);
	setSupportedRelationships(relationships);
}

void UnfocusArchiveEntry::onTrigger(ProcessContext *context, ProcessSession *session)
{
	FlowFileRecord *flowFile = session->get();

	if (!flowFile)
	{
		return;
	}

	// Get lens stack from attribute
	ArchiveMetadata lensArchiveMetadata;

	{
		Document doc;
		Document::AllocatorType &alloc = doc.GetAllocator();

		std::string existingLensStack;

		if (flowFile->getAttribute("lens.archive.stack", existingLensStack))
		{
			_logger->log_info("UnfocusArchiveEntry loading existing lens context");
			//TODO handle any exceptions that might arise from working with JSON data
			ParseResult result = doc.Parse(existingLensStack.c_str());

			if (!result)
			{
				_logger->log_error("UnfocusArchiveEntry JSON parse error: %s (%u)",
						GetParseError_En(result.Code()), result.Offset());
				context->yield();
				return;
			}
		}
		else
		{
			_logger->log_error("UnfocusArchiveEntry lens metadata not found");
			context->yield();
			return;
		}

		Value &metadataDoc = doc[doc.Size() - 1];
		doc.PopBack();

		lensArchiveMetadata.archiveFormatName.assign(metadataDoc["archive_format_name"].GetString());
		lensArchiveMetadata.archiveFormat = metadataDoc["archive_format"].GetUint64();
		lensArchiveMetadata.focusedEntry = metadataDoc["focused_entry"].GetString();

		for (auto itr = metadataDoc["archive_structure"].Begin();
				itr != metadataDoc["archive_structure"].End(); ++itr)
		{
			const auto &entryVal = itr->GetObject();
			ArchiveEntryMetadata metadata;
			metadata.tmpFileName = boost::filesystem::unique_path().native();
			metadata.entryName.assign(entryVal["entry_name"].GetString());
			metadata.entryType = entryVal["entry_type"].GetUint64();
			metadata.entryPerm = entryVal["entry_perm"].GetUint64();

			if (metadata.entryType == AE_IFREG)
			{
				metadata.stashKey.assign(entryVal["stash_key"].GetString());
			}

			lensArchiveMetadata.entryMetadata.push_back(metadata);
		}

	}

	// Export focused entry to tmp file
	for (const auto &entry : lensArchiveMetadata.entryMetadata)
	{
		if (entry.entryType != AE_IFREG)
		{
			continue;
		}

		if (entry.entryName == lensArchiveMetadata.focusedEntry)
		{
			session->exportContent(entry.tmpFileName, flowFile, false);
		}
	}

	// Restore/export entries from stash, one-by-one, to tmp files
	for (const auto &entry : lensArchiveMetadata.entryMetadata)
	{
		if (entry.entryType != AE_IFREG)
		{
			continue;
		}

		if (entry.entryName == lensArchiveMetadata.focusedEntry)
		{
			continue;
		}

		session->restore(entry.stashKey, flowFile);
		//TODO implement copy export/don't worry about multiple claims/optimal efficiency for *now*
		session->exportContent(entry.tmpFileName, flowFile, false);
	}

	// Create archive by restoring each entry in the archive from tmp files
	WriteCallback cb(&lensArchiveMetadata);
	session->write(flowFile, &cb);

	// Set new/updated lens stack to attribute; height(stack') = height(stack) - 1

	// if (!flowFile->updateAttribute("lens.archive.stack", stackStr))
	// {
	// 	flowFile->addAttribute("lens.archive.stack", stackStr);
	// }

    // Transfer to the relationship
    session->transfer(flowFile, Success);
}

UnfocusArchiveEntry::WriteCallback::WriteCallback(ArchiveMetadata *archiveMetadata)
{
	_logger = Logger::getLogger();
	_archiveMetadata = archiveMetadata;
}

typedef struct
{
	std::ofstream *stream;
} UnfocusArchiveEntryWriteData;

void UnfocusArchiveEntry::WriteCallback::process(std::ofstream *stream)
{
	auto outputArchive = archive_write_new();

	archive_write_set_format(outputArchive, _archiveMetadata->archiveFormat);

	UnfocusArchiveEntryWriteData data;
	data.stream = stream;

	auto open = [] (struct archive *, void *d) -> int
	{
		// Return OK because we wouldn't have gotten to this point if we didn't have a valid ofstream for a flow file
		return ARCHIVE_OK;
	};

	auto write = [] (struct archive *, void *d, const void *buffer, size_t length) -> ssize_t
	{
		auto data = static_cast<UnfocusArchiveEntryWriteData *>(d);
		data->stream->write(static_cast<const char *>(buffer), length);
		return length;
	};

	auto close = [] (struct archive *, void *d) -> int
	{
		// Return OK because we have successfully written to the stream by this point
		return ARCHIVE_OK;
	};

	archive_write_open(outputArchive, &data, open, write, close);

	// Iterate entries & write from tmp file to archive
	for (const auto &entryMetadata : _archiveMetadata->entryMetadata)
	{
		char buf[8192];
		int fd;
		struct stat st;
	    auto entry = archive_entry_new();

		_logger->log_info("UnfocusArchiveEntry writing entry %s",
				entryMetadata.entryName.c_str());

		archive_entry_set_filetype(entry, entryMetadata.entryType);
	    archive_entry_set_pathname(entry, entryMetadata.entryName.c_str());
	    archive_entry_set_perm(entry, entryMetadata.entryPerm);

	    // If entry is regular file, copy entry contents
		if (entryMetadata.entryType == AE_IFREG)
		{
			stat(entryMetadata.tmpFileName.c_str(), &st);
		    archive_entry_copy_stat(entry, &st);
	    	archive_write_header(outputArchive, entry);

			_logger->log_info("UnfocusArchiveEntry writing %d bytes of data from tmp file %s to archive entry %s",
					st.st_size,
					entryMetadata.tmpFileName.c_str(),
					entryMetadata.entryName.c_str());
			std::ifstream ifs(entryMetadata.tmpFileName, std::ifstream::in | std::ios::binary);

			while (ifs.good())
			{
				ifs.read(buf, sizeof(buf));
				auto len = ifs.gcount();
				if (archive_write_data(outputArchive, buf, len) < 0)
				{
					_logger->log_error("UnfocusArchiveEntry failed to write data to archive entry %s due to error: %s",
							entryMetadata.entryName.c_str(),
							archive_error_string(outputArchive));
				}
			}

			ifs.close();

			// Remove the tmp file as we are through with it
			std::remove(entryMetadata.tmpFileName.c_str());
		}
		else
		{
	    	archive_write_header(outputArchive, entry);
		}

	    archive_entry_free(entry);
	}

	archive_write_close(outputArchive);
	archive_write_free(outputArchive);
}
