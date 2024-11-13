// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DB_ITER_H_
#define STORAGE_LEVELDB_DB_DB_ITER_H_

#include <cstdint>

#include "db/dbformat.h"
#include "leveldb/db.h"
// 核心功能函数：

// NewDBIterator：创建一个数据库迭代器，用于将内部键（internal keys）转换为用户可见的键（user keys）。
// 功能参数：

// db：指向数据库实现（DBImpl）的指针。
// user_key_comparator：用于比较用户键的比较器。
// internal_iter：内部迭代器，用于访问底层存储的键值对。
// sequence：指定读取的序列号，确保一致性视图。
// seed：随机化操作的种子，可能用于调试或性能优化。
namespace leveldb {

class DBImpl;

// Return a new iterator that converts internal keys (yielded by
// "*internal_iter") that were live at the specified "sequence" number
// into appropriate user keys.
Iterator* NewDBIterator(DBImpl* db, const Comparator* user_key_comparator,
                        Iterator* internal_iter, SequenceNumber sequence,
                        uint32_t seed);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DB_ITER_H_
