// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.


#ifndef STORAGE_LEVELDB_DB_LOG_WRITER_H_
#define STORAGE_LEVELDB_DB_LOG_WRITER_H_

#include <cstdint>

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"
// 文件结构：
// 这个头文件是 LevelDB 日志系统的一部分，并与 WritableFile 一起工作，WritableFile 是用于写入数据到文件的抽象。日志系统对 LevelDB 的持久性和恢复功能至关重要。

// 关键概念：
// 日志记录类型：
// LevelDB 中的日志记录可以是不同类型的（例如，写入、删除），每种类型都具有相应的类型和校验和，以验证完整性。Writer 类高效地处理记录的不同类型。

// Slice 和 WritableFile：

// Slice 是一个简单的数据结构，表示对一段字节序列的视图（这里用来表示日志数据）。
// WritableFile 是一种可以写入数据的文件抽象，Writer 类使用它将数据追加到日志文件中。
// 校验和和完整性：
// Writer 类通过使用 CRC32C 校验和来确保数据完整性。这些校验和有助于验证在写入过程中数据是否被损坏。
namespace leveldb {

class WritableFile;

namespace log {

class Writer {
 public:
  // Create a writer that will append data to "*dest".
  // "*dest" must be initially empty.
  // "*dest" must remain live while this Writer is in use.
  explicit Writer(WritableFile* dest);

  // Create a writer that will append data to "*dest".
  // "*dest" must have initial length "dest_length".
  // "*dest" must remain live while this Writer is in use.
  Writer(WritableFile* dest, uint64_t dest_length);

  Writer(const Writer&) = delete;
  Writer& operator=(const Writer&) = delete;

  ~Writer();

  Status AddRecord(const Slice& slice);

 private:
  Status EmitPhysicalRecord(RecordType type, const char* ptr, size_t length);

  WritableFile* dest_;
  int block_offset_;  // Current offset in block

  // crc32c values for all supported record types.  These are
  // pre-computed to reduce the overhead of computing the crc of the
  // record type stored in the header.
  uint32_t type_crc_[kMaxRecordType + 1];
};

}  // namespace log
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_LOG_WRITER_H_
