// Copyright (c) 2013 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "gtest/gtest.h"
#include "db/db_impl.h"
#include "leveldb/cache.h"
#include "leveldb/db.h"
#include "util/testutil.h"

namespace leveldb {// leveldb 命名空间

class AutoCompactTest : public testing::Test {//AutoCompactTest 创建一个 AutoCompact的测试类
 public:
  AutoCompactTest() {
    dbname_ = testing::TempDir() + "autocompact_test";// 数据库路径 dbname_
    tiny_cache_ = NewLRUCache(100); // 设置小缓存  tiny_cache
    options_.block_cache = tiny_cache_; // 配置 options
    DestroyDB(dbname_, options_); // 防止数据库已经存在
    options_.create_if_missing = true;// 允许创建缺失的 数据库, 禁止使用压缩
    options_.compression = kNoCompression; // 设置选项
    EXPECT_LEVELDB_OK(DB::Open(options_, dbname_, &db_));// 打开数据库 , 检查是否成功
  }

  ~AutoCompactTest() {
    delete db_;
    DestroyDB(dbname_, Options());
    delete tiny_cache_;
  }

  std::string Key(int i) {// 生成一个键字符串, 格式为key000000 - key99999999 ....
    char buf[100];
    std::snprintf(buf, sizeof(buf), "key%06d", i);
    return std::string(buf);
  }

  uint64_t Size(const Slice& start, const Slice& limit) {//计算指定范围内的数据大小。
    Range r(start, limit);
    uint64_t size;
    db_->GetApproximateSizes(&r, 1, &size);
    return size;
  }

  void DoReads(int n);// 用于执行读取操作

 private:
  std::string dbname_;
  Cache* tiny_cache_;
  Options options_;
  DB* db_;
};

static const int kValueSize = 200 * 1024;// 每个值的大小 200kb
static const int kTotalSize = 100 * 1024 * 1024; // 总数据大小  100MB
static const int kCount = kTotalSize / kValueSize;

// Read through the first n keys repeatedly and check that they get
// compacted (verified by checking the size of the key space).
void AutoCompactTest::DoReads(int n) { //
  std::string value(kValueSize, 'x');// 创建一个固定大小的值字符串 value
  DBImpl* dbi = reinterpret_cast<DBImpl*>(db_); // 将db 转换成 dbimp

  // Fill database
  for (int i = 0; i < kCount; i++) {// 
    ASSERT_LEVELDB_OK(db_->Put(WriteOptions(), Key(i), value));// 循环插入 kCount记录, 强制压缩内存表
  }
  ASSERT_LEVELDB_OK(dbi->TEST_CompactMemTable());// 强制压缩内存表

  // Delete everything
  for (int i = 0; i < kCount; i++) {// 强制删除 kCount条记录
    ASSERT_LEVELDB_OK(db_->Delete(WriteOptions(), Key(i)));
  }
  ASSERT_LEVELDB_OK(dbi->TEST_CompactMemTable()); // 强制压缩 内存表

  // Get initial measurement of the space we will be reading.
  const int64_t initial_size = Size(Key(0), Key(n)); // 计算前n个键的初始化大小
  const int64_t initial_other_size = Size(Key(n), Key(kCount)); // 计算剩余键的大小

  // Read until size drops significantly.
  std::string limit_key = Key(n);
  for (int read = 0; true; read++) {
    ASSERT_LT(read, 100) << "Taking too long to compact";
    Iterator* iter = db_->NewIterator(ReadOptions());
    for (iter->SeekToFirst();
         iter->Valid() && iter->key().ToString() < limit_key; iter->Next()) {
      // Drop data
    }
    delete iter;
    // Wait a little bit to allow any triggered compactions to complete.
    Env::Default()->SleepForMicroseconds(1000000);
    uint64_t size = Size(Key(0), Key(n));
    std::fprintf(stderr, "iter %3d => %7.3f MB [other %7.3f MB]\n", read + 1,
                 size / 1048576.0, Size(Key(n), Key(kCount)) / 1048576.0);
    if (size <= initial_size / 10) {
      break;
    }
  }

  // Verify that the size of the key space not touched by the reads
  // is pretty much unchanged.
  const int64_t final_other_size = Size(Key(n), Key(kCount));
  ASSERT_LE(final_other_size, initial_other_size + 1048576);
  ASSERT_GE(final_other_size, initial_other_size / 5 - 1048576);
}

TEST_F(AutoCompactTest, ReadAll) { DoReads(kCount); }// 测试用例, 读取所有键

TEST_F(AutoCompactTest, ReadHalf) { DoReads(kCount / 2); } // 读取一半的键

}  // namespace leveldb
