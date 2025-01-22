// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_
#define STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_

#include "db/dbformat.h"
#include "leveldb/write_batch.h"

namespace leveldb {

class MemTable;

// WriteBatchInternal provides static methods for manipulating a
// WriteBatch that we don't want in the public WriteBatch interface.
class WriteBatchInternal {
 public:
  static int Count(const WriteBatch* batch);// Count 方法返回 WriteBatch 中的条目数。

  static void SetCount(WriteBatch* batch, int n);// SetCount 方法设置 WriteBatch 中的条目数。

  static SequenceNumber Sequence(const WriteBatch* batch);// Sequence 方法返回 WriteBatch 的起始序列号。

  static void SetSequence(WriteBatch* batch, SequenceNumber seq);// SetSequence 方法设置 WriteBatch 的起始序列号。

  static Slice Contents(const WriteBatch* batch) { return Slice(batch->rep_); }// 返回当前的 rep

  static size_t ByteSize(const WriteBatch* batch) { return batch->rep_.size(); }// 返回

  static void SetContents(WriteBatch* batch, const Slice& contents);

  static Status InsertInto(const WriteBatch* batch, MemTable* memtable);

  static void Append(WriteBatch* dst, const WriteBatch* src);
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_WRITE_BATCH_INTERNAL_H_
