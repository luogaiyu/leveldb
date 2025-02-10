// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_

#include <cstdint>
#include <vector>

#include "leveldb/slice.h"

namespace leveldb {

struct Options;

class BlockBuilder {
 public:
  explicit BlockBuilder(const Options* options);
  // 防止内存泄漏: 控制对象只能通过初始化函数创建, 不能通过克隆或者赋值操作
  BlockBuilder(const BlockBuilder&) = delete;
  BlockBuilder& operator=(const BlockBuilder&) = delete;

  // 重置内容就像是 BlockBuilder 才刚创建一样
  void Reset();


  // 要求: Finsh() 没有被调用 从上次调用Reset() 方法之后
  // 要求: key 要比之前添加的key 都大
  void Add(const Slice& key, const Slice& value);

  // 完成 构建block和返回 slice()
  // Finish building the block and return a slice that refers to the
  // block contents.  The returned slice will remain valid for the
  // lifetime of this builder or until Reset() is called.
  Slice Finish();

  // Returns an estimate of the current (uncompressed) size of the block
  // we are building.
  size_t CurrentSizeEstimate() const;

  // Return true iff no entries have been added since the last Reset()
  bool empty() const { return buffer_.empty(); }

 private:
  const Options* options_;          // 选项: 用于提供参数控制block创建block的过程
  std::string buffer_;              // 目标缓存: 使用string的原因, 提供了自动管理内存的方式? 不用再手动进行管理
  std::vector<uint32_t> restarts_;  // Restart points
  int counter_;                     // 从重启开始有多少的entris被忽略了
  bool finished_;                   // Finish() 这个方法是否已经被调用
  std::string last_key_;            // 上一次操作的key
};

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
