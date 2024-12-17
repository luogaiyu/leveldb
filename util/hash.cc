// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/hash.h"

#include <cstring>

#include "util/coding.h"

// The FALLTHROUGH_INTENDED macro can be used to annotate implicit fall-through
// between switch labels. The real definition should be provided externally.
// This one is a fallback version for unsupported compilers.
#ifndef FALLTHROUGH_INTENDED
#define FALLTHROUGH_INTENDED \
  do {                       \
  } while (0)
#endif

namespace leveldb {
/**
 * data: 指向 要hash数据的指针
 *  n: 数据的长度
 *  seed: 不同的种子
 */
uint32_t Hash(const char* data, size_t n, uint32_t seed) {// 实现hash方法
  // Similar to murmur hash
  const uint32_t m = 0xc6a4a793;// 乘法常量
  const uint32_t r = 24; // 右移位数
  const char* limit = data + n; // 输入数据的结束位置
  uint32_t h = seed ^ (n * m);  // 初始化hash值
/**
 * 这段代码的优点是什么?
 * 1. 分块处理能够更好的运用并行计算?
 * 2. 使用 乘法, 异或, 移位
 * 3. 
 */
  // Pick up four bytes at a time
  while (data + 4 <= limit) {
    uint32_t w = DecodeFixed32(data); // 读取4字节数据
    data += 4; // 移动指针
    h += w; // 将读取的4字节数据加到hash值上
    h *= m; // 乘法混合
    h ^= (h >> 16); //异或混合
  }

  // 处理剩余的三个字节
  switch (limit - data) {
    case 3: 
      h += static_cast<uint8_t>(data[2]) << 16;
      FALLTHROUGH_INTENDED;
    case 2://
      h += static_cast<uint8_t>(data[1]) << 8;
      FALLTHROUGH_INTENDED;
    case 1:
      h += static_cast<uint8_t>(data[0]);
      h *= m;
      h ^= (h >> r);
      break;
  }
  return h;
}

}  // namespace leveldb
