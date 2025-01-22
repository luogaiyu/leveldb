// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_FORMAT_H_
#define STORAGE_LEVELDB_TABLE_FORMAT_H_

#include <cstdint>
#include <string>

#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/table_builder.h"
/**
 * 
format.h 文件定义了 BlockHandle 和 Footer 类，以及 ReadBlock 函数的声明。这些类和函数用于管理和解析 SSTable 文件中的块和元数据。主要功能包括：

BlockHandle：表示文件中数据块或元数据块的位置，提供编码和解码方法。
Footer：表示 SSTable 文件末尾的固定信息，包含元索引块和索引块的 BlockHandle，提供编码和解码方法。
ReadBlock：从文件中读取指定的块，解压（如果需要）并返回块的内容。
这些类和函数确保了 LevelDB 能够高效地管理和访问 SSTable 文件中的数据。

引用

 */
namespace leveldb {

class Block;
class RandomAccessFile;
struct ReadOptions;

// 用于标识数据块在文件中的位置和大小, 用于索引和元数据块的管理, 帮助快速定位和读取数据块
class BlockHandle {
 public:
  // BlockHandle最大编码长度
  enum { kMaxEncodedLength = 10 + 10 };

  BlockHandle();

  // offset: 偏移量
  uint64_t offset() const { return offset_; }
  void set_offset(uint64_t offset) { offset_ = offset; }

  // size: 大小
  uint64_t size() const { return size_; }
  void set_size(uint64_t size) { size_ = size; }

  void EncodeTo(std::string* dst) const;
  Status DecodeFrom(Slice* input);

 private:
  uint64_t offset_;
  uint64_t size_;
};

/**
 * 定义Footer: 表示LevelDB表文件的尾部信息
 * 包含 表文件中 元索引块 和索引块的位置信息
 */
class Footer {
 public:
 // 表示Footer 的编码长度, 由两个BlockHandle的最大编码长度和一个8字节的魔数
 // 在 C++ 中，枚举值（enum）确实可以被视为静态常量，即使它们没有显式地使用 static 关键字。这是因为枚举值在编译时就被确定，并且属于类的静态成员。
  enum { kEncodedLength = 2 * BlockHandle::kMaxEncodedLength + 8 };
  // 默认无参构造函数
  Footer() = default;

  
  const BlockHandle& metaindex_handle() const { return metaindex_handle_; }// 返回元索引块的句柄 
  void set_metaindex_handle(const BlockHandle& h) { metaindex_handle_ = h; }// 设置索引块的句柄。指向具体的 数据

  const BlockHandle& index_handle() const { return index_handle_; } // 返回索引块的句柄。
  void set_index_handle(const BlockHandle& h) { index_handle_ = h; } // 设置元索引块的句柄, 就是指向 元数据

  void EncodeTo(std::string* dst) const;// 使用Encode方法来进行编码
  Status DecodeFrom(Slice* input); //解码

 private:
  BlockHandle metaindex_handle_;
  BlockHandle index_handle_;
};

// kTableMagicNumber was picked by running
//    echo http://code.google.com/p/leveldb/ | sha1sum
// and taking the leading 64 bits.
static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;// 魔数

// 1-byte type + 32-bit crc
static const size_t kBlockTrailerSize = 5;// 设置block的尾部大小, 1字节:类型 + 4字节crc校验

struct BlockContents {
  Slice data;           // 数据
  bool cachable;        // 是否可以缓存
  bool heap_allocated;  // 是否需要调用者删除数据
};

// 从 file 中 读取由  handle 标识的块, 如果失败, 返回非 OK, 如果成功, 填充 *result 并返回 OK
Status ReadBlock(RandomAccessFile* file, const ReadOptions& options,
                 const BlockHandle& handle, BlockContents* result);

// inline : 建议编译器将函数内联展开
inline BlockHandle::BlockHandle()
    : offset_(~static_cast<uint64_t>(0)), size_(~static_cast<uint64_t>(0)) {}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_FORMAT_H_
