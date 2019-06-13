/**
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

#include "io/FileMemoryMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace org {
namespace apache {
namespace nifi {
namespace minifi {
namespace io {

FileMemoryMap::FileMemoryMap(const std::string &path, size_t map_size, bool read_only)
    : file_data_(nullptr), path_(path), length_(map_size), read_only_(read_only), logger_(logging::LoggerFactory<FileMemoryMap>::getLogger()) {
  // open the file
  if (!read_only) {
    fd_ = open(path.c_str(), O_RDWR | O_CREAT, 0600);
  } else {
    fd_ = open(path.c_str(), O_RDONLY | O_CREAT, 0600);
  }

  if (fd_ < 0) {
    throw std::runtime_error("Failed to open for memory mapping: " + path);
  }

  // ensure file is at least as big as requested map size
  if (!read_only) {
    if (lseek(fd_, map_size, SEEK_SET) < 0) {
      throw std::runtime_error("Failed to seek " + std::to_string(map_size) + " bytes for mapping: " + path);
    }

    if (write(fd_, "", 1) < 0) {
      close(fd_);
      throw std::runtime_error("Failed to write 0 byte at end of file to expand file: " + path);
    }
  }

  // memory map the file
  if (read_only) {
    file_data_ = mmap(nullptr, map_size, PROT_READ, MAP_SHARED | MAP_POPULATE, fd_, 0);
  } else {
    file_data_ = mmap(nullptr, map_size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_POPULATE, fd_, 0);
  }

  if (file_data_ == MAP_FAILED) {
    throw std::runtime_error("Failed to memory map file: " + path);
  }
}

void FileMemoryMap::unmap() {
  if (file_data_ != nullptr) {
    if (munmap(file_data_, length_) != 0) {
      if (fd_ > 0) {
        close(fd_);
      }
      throw std::runtime_error("Failed to memory unmap file: " + path_);
    }
  }

  if (fd_ > 0) {
    close(fd_);
  }

  fd_ = -1;
  file_data_ = nullptr;
}

void *FileMemoryMap::getData() { return file_data_; }

size_t FileMemoryMap::getSize() { return length_; }

void *FileMemoryMap::resize(size_t new_size) {
  if (file_data_ == nullptr) {
    throw std::runtime_error("Cannot resize unmapped file: " + path_);
  }

  auto new_data = mremap(file_data_, length_, new_size, MREMAP_MAYMOVE);

  if (new_data == MAP_FAILED || new_data == nullptr) {
    if (fd_ > 0) {
      close(fd_);
    }
    throw std::runtime_error("Failed to memory remap file: " + path_);
  }

  file_data_ = new_data;
  length_ = new_size;
  return new_data;
}

}  // namespace io
}  // namespace minifi
}  // namespace nifi
}  // namespace apache
}  // namespace org
