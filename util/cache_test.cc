// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

// 引入当前的数据
#include "leveldb/cache.h"

#include <vector>

#include "gtest/gtest.h"
#include "util/coding.h"

// 命名空间
namespace leveldb {

// Conversions between numeric keys/values and the types expected by Cache.
static std::string EncodeKey(int k) {// 将整数k 编码为 32位 固定长度的字节序列, 返回一个 std:: string
  std::string result;
  PutFixed32(&result, k);
  return result;
}
static int DecodeKey(const Slice& k) {// 将 Slice 的32位 固定长度的字节序列 解码成整数
  assert(k.size() == 4);
  return DecodeFixed32(k.data());
}

static void* EncodeValue(uintptr_t v) { return reinterpret_cast<void*>(v); }
static int DecodeValue(void* v) { return reinterpret_cast<uintptr_t>(v); }

class CacheTest : public testing::Test {// 定义一个类 CacheTest 继承  Test
 public:
  static void Deleter(const Slice& key, void* v) {// 用于删除 缓存项时 删除的键和值
    current_->deleted_keys_.push_back(DecodeKey(key));
    current_->deleted_values_.push_back(DecodeValue(v));
  }

  static constexpr int kCacheSize = 1000;// 表示 缓存大小 1000
  std::vector<int> deleted_keys_; // 用于记录 删除的键和值
  std::vector<int> deleted_values_;
  Cache* cache_;// 指向缓存对象

  CacheTest() : cache_(NewLRUCache(kCacheSize)) { current_ = this; }// 指向当前对象

  ~CacheTest() { delete cache_; }// 定义析构函数 释放缓存对象

  int Lookup(int key) { // 用于查看缓存中的key 
    Cache::Handle* handle = cache_->Lookup(EncodeKey(key));
    const int r = (handle == nullptr) ? -1 : DecodeValue(cache_->Value(handle));
    if (handle != nullptr) {
      cache_->Release(handle);
    }
    return r;
  }

  void Insert(int key, int value, int charge = 1) {// 用于将键值对插入缓存, charge
    cache_->Release(cache_->Insert(EncodeKey(key), EncodeValue(value), charge,
                                   &CacheTest::Deleter));
  }

  Cache::Handle* InsertAndReturnHandle(int key, int value, int charge = 1) {
    return cache_->Insert(EncodeKey(key), EncodeValue(value), charge,
                          &CacheTest::Deleter);
  }

  void Erase(int key) { cache_->Erase(EncodeKey(key)); }// 用于在缓存中 删除指定的键
  static CacheTest* current_;
};
CacheTest* CacheTest::current_;

TEST_F(CacheTest, HitAndMiss) {// 测试 命中和未命中的情况
  ASSERT_EQ(-1, Lookup(100));

  Insert(100, 101);
  ASSERT_EQ(101, Lookup(100));
  ASSERT_EQ(-1, Lookup(200));
  ASSERT_EQ(-1, Lookup(300));

  Insert(200, 201);
  ASSERT_EQ(101, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(-1, Lookup(300));

  Insert(100, 102);
  ASSERT_EQ(102, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(-1, Lookup(300));

  ASSERT_EQ(1, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[0]);
  ASSERT_EQ(101, deleted_values_[0]);
}

TEST_F(CacheTest, Erase) {// 测试 删除key
  Erase(200);
  ASSERT_EQ(0, deleted_keys_.size());

  Insert(100, 101);
  Insert(200, 201);
  Erase(100);
  ASSERT_EQ(-1, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(1, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[0]);
  ASSERT_EQ(101, deleted_values_[0]);

  Erase(100);
  ASSERT_EQ(-1, Lookup(100));
  ASSERT_EQ(201, Lookup(200));
  ASSERT_EQ(1, deleted_keys_.size());
}

TEST_F(CacheTest, EntriesArePinned) { // 测试缓存项中 固定的情况
  Insert(100, 101);
  Cache::Handle* h1 = cache_->Lookup(EncodeKey(100));
  ASSERT_EQ(101, DecodeValue(cache_->Value(h1)));

  Insert(100, 102);
  Cache::Handle* h2 = cache_->Lookup(EncodeKey(100));
  ASSERT_EQ(102, DecodeValue(cache_->Value(h2)));
  ASSERT_EQ(0, deleted_keys_.size());

  cache_->Release(h1);
  ASSERT_EQ(1, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[0]);
  ASSERT_EQ(101, deleted_values_[0]);

  Erase(100);
  ASSERT_EQ(-1, Lookup(100));
  ASSERT_EQ(1, deleted_keys_.size());

  cache_->Release(h2);
  ASSERT_EQ(2, deleted_keys_.size());
  ASSERT_EQ(100, deleted_keys_[1]);
  ASSERT_EQ(102, deleted_values_[1]);
}

TEST_F(CacheTest, EvictionPolicy) {// 测试缓存逐出的策略
  Insert(100, 101);
  Insert(200, 201);
  Insert(300, 301);
  Cache::Handle* h = cache_->Lookup(EncodeKey(300));

  // Frequently used entry must be kept around,
  // as must things that are still in use.
  for (int i = 0; i < kCacheSize + 100; i++) {
    Insert(1000 + i, 2000 + i);
    ASSERT_EQ(2000 + i, Lookup(1000 + i));
    ASSERT_EQ(101, Lookup(100));
  }
  ASSERT_EQ(101, Lookup(100));
  ASSERT_EQ(-1, Lookup(200));
  ASSERT_EQ(301, Lookup(300));
  cache_->Release(h);
}

TEST_F(CacheTest, UseExceedsCacheSize) {// 测试缓存超出容量的情况
  // Overfill the cache, keeping handles on all inserted entries.
  std::vector<Cache::Handle*> h;
  for (int i = 0; i < kCacheSize + 100; i++) {
    h.push_back(InsertAndReturnHandle(1000 + i, 2000 + i));
  }

  // Check that all the entries can be found in the cache.
  for (int i = 0; i < h.size(); i++) {
    ASSERT_EQ(2000 + i, Lookup(1000 + i));
  }

  for (int i = 0; i < h.size(); i++) {
    cache_->Release(h[i]);
  }
}

TEST_F(CacheTest, HeavyEntries) {// 测试
  // Add a bunch of light and heavy entries and then count the combined
  // size of items still in the cache, which must be approximately the
  // same as the total capacity.
  const int kLight = 1;
  const int kHeavy = 10;
  int added = 0;
  int index = 0;
  while (added < 2 * kCacheSize) {
    const int weight = (index & 1) ? kLight : kHeavy;
    Insert(index, 1000 + index, weight);
    added += weight;
    index++;
  }

  int cached_weight = 0;
  for (int i = 0; i < index; i++) {
    const int weight = (i & 1 ? kLight : kHeavy);
    int r = Lookup(i);
    if (r >= 0) {
      cached_weight += weight;
      ASSERT_EQ(1000 + i, r);
    }
  }
  ASSERT_LE(cached_weight, kCacheSize + kCacheSize / 10);
}

TEST_F(CacheTest, NewId) {
  uint64_t a = cache_->NewId();
  uint64_t b = cache_->NewId();
  ASSERT_NE(a, b);
}

TEST_F(CacheTest, Prune) {
  Insert(1, 100);
  Insert(2, 200);

  Cache::Handle* handle = cache_->Lookup(EncodeKey(1));
  ASSERT_TRUE(handle);
  cache_->Prune();
  cache_->Release(handle);

  ASSERT_EQ(100, Lookup(1));
  ASSERT_EQ(-1, Lookup(2));
}

TEST_F(CacheTest, ZeroSizeCache) { // 测试缓存大小为0的情况
  delete cache_;
  cache_ = NewLRUCache(0);

  Insert(1, 100);
  ASSERT_EQ(-1, Lookup(1));
}

}  // namespace leveldb
