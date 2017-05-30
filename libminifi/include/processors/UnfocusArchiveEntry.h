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
#ifndef LIBMINIFI_INCLUDE_PROCESSORS_UNFOCUSARCHIVEENTRY_H_
#define LIBMINIFI_INCLUDE_PROCESSORS_UNFOCUSARCHIVEENTRY_H_

#include <memory>
#include <string>

#include "FocusArchiveEntry.h"
#include "FlowFileRecord.h"
#include "core/Processor.h"
#include "core/ProcessSession.h"
#include "core/Core.h"
#include "core/logging/Logger.h"
#include "core/Resource.h"

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace processors {

using logging::Logger;

//! UnfocusArchiveEntry Class
class UnfocusArchiveEntry : public core::Processor {
 public:
  //! Constructor
  /*!
   * Create a new processor
   */
  explicit UnfocusArchiveEntry(std::string name, uuid_t uuid = NULL)
  : core::Processor(name, uuid) {
    _logger = Logger::getLogger();
  }
  //! Destructor
  virtual ~UnfocusArchiveEntry() {
  }
  //! Processor Name
  static const std::string ProcessorName;
  //! Supported Relationships
  static core::Relationship Success;

  //! OnTrigger method, implemented by NiFi UnfocusArchiveEntry
  virtual void onTrigger(core::ProcessContext *context,
      core::ProcessSession *session);
  //! Initialize, over write by NiFi UnfocusArchiveEntry
  virtual void initialize(void);

  typedef FocusArchiveEntry::ArchiveEntryMetadata ArchiveEntryMetadata;
  typedef FocusArchiveEntry::ArchiveMetadata ArchiveMetadata;

  //! Write callback for reconstituting lensed archive into flow file content
  class WriteCallback : public OutputStreamCallback {
   public:
    explicit WriteCallback(ArchiveMetadata *archiveMetadata);
    void process(std::ofstream *stream);

   private:
    //! Logger
    std::shared_ptr<Logger> _logger;
    ArchiveMetadata *_archiveMetadata;
  };

 private:
  //! Logger
  std::shared_ptr<Logger> _logger;
};

REGISTER_RESOURCE(UnfocusArchiveEntry);

} /* namespace processors */
} /* namespace minifi */
} /* namespace nifi */
} /* namespace apache */
} /* namespace org */

#endif  // LIBMINIFI_INCLUDE_PROCESSORS_UNFOCUSARCHIVEENTRY_H_
