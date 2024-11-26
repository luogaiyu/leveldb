// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Thread-safe (provides internal synchronization)

#ifndef STORAGE_LEVELDB_DB_TABLE_CACHE_H_
#define STORAGE_LEVELDB_DB_TABLE_CACHE_H_

#include <cstdint>
#include <string>

#include "db/dbformat.h"
#include "leveldb/cache.h"
#include "leveldb/table.h"
#include "port/port.h"

// 核心功能
// 文件缓存管理：

// TableCache 是一个缓存层，专门用于管理磁盘上的 SSTable 文件，提供快速的读取操作。
// 实现：
// 它是一个 LRU 缓存，缓存了 SSTable 的元数据和数据块，以减少磁盘 I/O。
// TableCache 内部维护了一个文件句柄缓存和一个 BlockCache（用于缓存 SSTable 数据块）。
// NewIterator：为指定文件创建迭代器，支持数据读取操作。
// Get：在指定文件中查找键值对。
// Evict：从缓存中移除指定文件的记录。
// 线程安全：

// 内部实现了必要的同步机制，支持并发访问。
namespace leveldb {

class Env;

class TableCache {
 public:
  TableCache(const std::string& dbname, const Options& options, int entries);

  TableCache(const TableCache&) = delete;
  TableCache& operator=(const TableCache&) = delete;

  ~TableCache();

  // Return an iterator for the specified file number (the corresponding
  // file length must be exactly "file_size" bytes).  If "tableptr" is
  // non-null, also sets "*tableptr" to point to the Table object
  // underlying the returned iterator, or to nullptr if no Table object
  // underlies the returned iterator.  The returned "*tableptr" object is owned
  // by the cache and should not be deleted, and is valid for as long as the
  // returned iterator is live.
  Iterator* NewIterator(const ReadOptions& options, uint64_t file_number,
                        uint64_t file_size, Table** tableptr = nullptr);

  // If a seek to internal key "k" in specified file finds an entry,
  // call (*handle_result)(arg, found_key, found_value).
  Status Get(const ReadOptions& options, uint64_t file_number,
             uint64_t file_size, const Slice& k, void* arg,
             void (*handle_result)(void*, const Slice&, const Slice&));

  // Evict any entry for the specified file number
  void Evict(uint64_t file_number);

 private:
  Status FindTable(uint64_t file_number, uint64_t file_size, Cache::Handle**);

  Env* const env_;
  const std::string dbname_;
  const Options& options_;
  Cache* cache_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_TABLE_CACHE_H_
