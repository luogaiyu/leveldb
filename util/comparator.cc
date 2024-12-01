// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

// 定义当前的 Comparator 接口
#include "leveldb/comparator.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <type_traits>
// 包含了一些标准库头文件，用于提供算法、整数类型、字符串和类型特性等功能。
#include "leveldb/slice.h"
#include "util/logging.h"
#include "util/no_destructor.h"
// 包含了 LevelDB 项目中的其他头文件，分别定义了 Slice 类、日志记录功能和 NoDestructor 模板类。
namespace leveldb {// 定义命名空间

Comparator::~Comparator() = default;// 定义 comparator类的默认析构函数

namespace { // 开始匿名的命名空间
class BytewiseComparatorImpl : public Comparator {// 创建
 public:
  BytewiseComparatorImpl() = default;

  const char* Name() const override { return "leveldb.BytewiseComparator"; } // 主要是重写了Name

  int Compare(const Slice& a, const Slice& b) const override {
    return a.compare(b);// 使用Slice类的 compare方法 来比较两个类
  }

  void FindShortestSeparator(std::string* start,
                             const Slice& limit) const override {
    // Find length of common prefix
    // 找到 start 和limit 之间的最短分隔符
    size_t min_length = std::min(start->size(), limit.size());
    size_t diff_index = 0;
    while ((diff_index < min_length) &&
           ((*start)[diff_index] == limit[diff_index])) {
      diff_index++;
    }

    if (diff_index >= min_length) {
      // Do not shorten if one string is a prefix of the other
    } else {
      uint8_t diff_byte = static_cast<uint8_t>((*start)[diff_index]);
      if (diff_byte < static_cast<uint8_t>(0xff) &&
          diff_byte + 1 < static_cast<uint8_t>(limit[diff_index])) {
        (*start)[diff_index]++;
        start->resize(diff_index + 1);
        assert(Compare(*start, limit) < 0);
      }
    }
  }

  void FindShortSuccessor(std::string* key) const override {// 用于找到  key的下一个字典序最小的字符串
    // Find first character that can be incremented
    size_t n = key->size();
    for (size_t i = 0; i < n; i++) {
      const uint8_t byte = (*key)[i];
      if (byte != static_cast<uint8_t>(0xff)) {
        (*key)[i] = byte + 1;
        key->resize(i + 1);
        return;
      }
    }
    // *key is a run of 0xffs.  Leave it alone.
  }
};
}  // namespace

const Comparator* BytewiseComparator() {//返回 BytewiseComparatorImpl 的单例实例。
  static NoDestructor<BytewiseComparatorImpl> singleton;// 类模版. 用于创建单例对象
  return singleton.get();// 单例实例
}

}  // namespace leveldb
