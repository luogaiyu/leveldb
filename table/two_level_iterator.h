// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_
#define STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_

#include "leveldb/iterator.h"

namespace leveldb {

struct ReadOptions;
/**
 * leveldb/table/two_level_iterator.cc 文件实现了 TwoLevelIterator 类，这是一个两层迭代器，用于遍历索引块和数据块。主要方法包括 Seek、SeekToFirst、SeekToLast、Next 和 Prev，用于导航迭代器。辅助方法 SkipEmptyDataBlocksForward 和 SkipEmptyDataBlocksBackward 用于跳过空的数据块。InitDataBlock 方法用于初始化数据块。NewTwoLevelIterator 函数用于创建一个新的 TwoLevelIterator 对象。这些方法和函数确保了 LevelDB 能够高效地管理和使用两层迭代器。
 */
// Return a new two level iterator.  A two-level iterator contains an
// index iterator whose values point to a sequence of blocks where
// each block is itself a sequence of key,value pairs.  The returned
// two-level iterator yields the concatenation of all key/value pairs
// in the sequence of blocks.  Takes ownership of "index_iter" and
// will delete it when no longer needed.
//
// Uses a supplied function to convert an index_iter value into
// an iterator over the contents of the corresponding block.
Iterator* NewTwoLevelIterator(
    Iterator* index_iter,
    Iterator* (*block_function)(void* arg, const ReadOptions& options,
                                const Slice& index_value),
    void* arg, const ReadOptions& options);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_TWO_LEVEL_ITERATOR_H_
