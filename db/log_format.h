// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Log format information shared by reader and writer.
// See ../doc/log_format.md for more detail.

#ifndef STORAGE_LEVELDB_DB_LOG_FORMAT_H_
#define STORAGE_LEVELDB_DB_LOG_FORMAT_H_

namespace leveldb {
namespace log {
/**
 * 定义了一个枚举 RecordType，用于表示日志记录的类型：
 * kZeroType：值为 0，保留用于预分配的文件。
 * kFullType：值为 1，表示完整记录。
 * kFirstType：值为 2，表示记录片段的开始部分。
 * kMiddleType：值为 3，表示记录片段的中间部分。
 * kLastType：值为 4，表示记录片段的结束部分。
 * 
 */
enum RecordType {
  // Zero is reserved for preallocated files
  kZeroType = 0,

  kFullType = 1,

  // For fragments
  kFirstType = 2,
  kMiddleType = 3,
  kLastType = 4
};
static const int kMaxRecordType = kLastType;

static const int kBlockSize = 32768;// 其值为 32768，表示日志块的大小

// Header is checksum (4 bytes), length (2 bytes), type (1 byte).
static const int kHeaderSize = 4 + 2 + 1;// kHeaderSize设置对应的头文件大小

}  // namespace log
}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_LOG_FORMAT_H_
