// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "util/arena.h"
// google Test 框架
#include "gtest/gtest.h"
// 定义随机数生成器Random
#include "util/random.h"

// leveldb 命名空间中
namespace leveldb {

TEST(ArenaTest, Empty) { Arena arena; }// ArenaTest 是组名称, Empty: 是测试用例的名称 定义简单测试用例, 创造空的 areana 对象

TEST(ArenaTest, Simple) { // 简单测试
  std::vector<std::pair<size_t, char*>> allocated; // 使用 Vector 来做容器, pair 模版类: 可以存储任意类型的两个值, char* 在使用中类似字符串 char* 需要手动管理内存
  Arena arena;
  const int N = 100000;
  size_t bytes = 0;
  Random rnd(301);// 随机数生成, 种子为301
  for (int i = 0; i < N; i++) {
    size_t s;
    if (i % (N / 10) == 0) {
      s = i;
    } else {
      s = rnd.OneIn(4000) // 表示某个事件发生的概率是 1/4000
              ? rnd.Uniform(6000) // 生成0-6000
              : (rnd.OneIn(10) ? rnd.Uniform(100) : rnd.Uniform(20));
    }
    if (s == 0) {
      // Our arena disallows size 0 allocations.
      s = 1; // arena 不允许 分配0字节的内存?
    }
    char* r;
    if (rnd.OneIn(10)) {
      r = arena.AllocateAligned(s);// 使用对齐分配
    } else {
      r = arena.Allocate(s); //普通内存分配
    }
    /**
     * 不同类型的处理器和编译器对内存对齐的要求可能不同。常见的对齐要求包括：
        8位（1字节）类型：通常不需要对齐。
        16位（2字节）类型：通常需要2字节对齐。
        32位（4字节）类型：通常需要4字节对齐。
        64位（8字节）类型：通常需要8字节对齐。
     * 
     */

    for (size_t b = 0; b < s; b++) {
      // Fill the "i"th allocation with a known bit pattern
      r[b] = i % 256; // 将已经分配的内存块 填充为已知的比特模式
    }
    bytes += s;//更新 已分配的总字节数
    allocated.push_back(std::make_pair(s, r));// 添加当前的信息到已分配
    ASSERT_GE(arena.MemoryUsage(), bytes); //  是否当前的内存使用 >= 总内存, 
    if (i > N / 10) {
      ASSERT_LE(arena.MemoryUsage(), bytes * 1.10); // 判断 当前内存使用量 <= 总内存
    }
  }
  for (size_t i = 0; i < allocated.size(); i++) {
    size_t num_bytes = allocated[i].first;
    const char* p = allocated[i].second;
    for (size_t b = 0; b < num_bytes; b++) {
      // Check the "i"th allocation for the known bit pattern
      ASSERT_EQ(int(p[b]) & 0xff, i % 256); //判断是否 内存中的对象是否能够和指标相等, 为了防止
    }
  }
}

}  // namespace leveldb
