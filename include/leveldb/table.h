// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_TABLE_H_
#define STORAGE_LEVELDB_INCLUDE_TABLE_H_

#include <cstdint>

#include "leveldb/export.h"
#include "leveldb/iterator.h"
// 
namespace leveldb {

class Block; //Block: 存储数据一个实体
class BlockHandle; // 操控Block 的一个句柄
class Footer; // 尾部信息
struct Options; // 
class RandomAccessFile;
struct ReadOptions;
class TableCache;//缓存

class LEVELDB_EXPORT Table {
 public:
 // 表读取方法
  static Status Open(const Options& options, RandomAccessFile* file,
                     uint64_t file_size, Table** table);

  /**
   * 赋值和克隆方法删除, ban掉
   */
  Table(const Table&) = delete;
  Table& operator=(const Table&) = delete;
  // 析构函数
  ~Table();
 /**
  * 返回一个新的迭代器, 用于遍历表的内容
  * NewIterator 最初返回的内容是无效的, 必须调用某个seek方法
  */
  Iterator* NewIterator(const ReadOptions&) const;

  /**
   * 返回的值是以文件字节为单位的，因此包括了底层数据的压缩等影响。
   * 例如，表中最后一个键的大致偏移量将接近文件长度。
   * 给定一个键，返回该键在文件中数据开始处的大致字节偏移量（或者如果该键存在于文件中的话，其数据开始处的大致字节偏移量）。 
   * */ 
  uint64_t ApproximateOffsetOf(const Slice& key) const;

 private:
  friend class TableCache; // 使用友元机制 来定义 表缓存, 主要的目的是 为了让 TableCache 类型 能够访问 table中的所有私有变量
  struct Rep;// 使用 Rep = Representation (表示或实现)

  static Iterator* BlockReader(void*, const ReadOptions&, const Slice&);// block是存储系统中的一环

  explicit Table(Rep* rep) : rep_(rep) {}

  /**
   * 调用 (*handle_result)(arg, ...)，传入在调用 Seek(key) 后找到的条目。
   * 如果过滤策略表明该键不存在，则可能不会进行这样的调用。
   */
  Status InternalGet(const ReadOptions&, const Slice& key, void* arg,
                     void (*handle_result)(void* arg, const Slice& k,
                                           const Slice& v));
  // 读取元数据信息
  void ReadMeta(const Footer& footer);
  // 读取过滤信息
  void ReadFilter(const Slice& filter_handle_value);

  Rep* const rep_;
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_TABLE_H_
