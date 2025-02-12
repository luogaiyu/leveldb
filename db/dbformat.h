// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DBFORMAT_H_
#define STORAGE_LEVELDB_DB_DBFORMAT_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"


// 代码重点
// 1. 配置参数 (config 命名空间)
// 定义了与存储和压缩相关的常量：
// kNumLevels：多级存储的层数，默认为 7。
// kL0_CompactionTrigger：触发 Level-0 层文件压缩的文件数量。
// kL0_StopWritesTrigger：当 Level-0 文件数量超过此值时，停止写操作。
// kMaxMemCompactLevel：MemTable 写入磁盘时，优先推送到的目标层级。
// kReadBytesPeriod：迭代过程中采样数据读取的字节间隔。
// 2. InternalKey 和内部键的处理
// InternalKey 的意义
// 用户键 (user_key) 与 元信息（序列号、操作类型）组合而成的键。
// 内部键的用途：
// 内部排序时优先按用户键排序，冲突时按序列号（从高到低）排序。
// 区分 Put 和 Delete 类型操作（kTypeValue 和 kTypeDeletion）。
// InternalKey 的主要功能
// 内部键编码与解码：
// AppendInternalKey：序列化 ParsedInternalKey。
// ParseInternalKey：反序列化 Slice，解析出 ParsedInternalKey 的组成部分。
// ExtractUserKey：从内部键中提取用户键。
// InternalKeyEncodingLength：获取编码长度（用户键长度 + 8 字节元数据）。
// 字符串化：
// DebugString：生成可读字符串，用于调试。
// ParsedInternalKey 结构
// 表示解码后的内部键，包含：
// user_key：用户键。
// sequence：序列号，用于排序。
// type：键类型（删除或存储）。
// 3. 内部比较器 (InternalKeyComparator)
// 核心功能：比较两个内部键。
// 用户键比较优先：调用用户定义的 Comparator。
// 序列号降序：当用户键相同时，序列号较大的键更小。
// 接口实现：
// Compare：比较 Slice 或 InternalKey。
// FindShortestSeparator 和 FindShortSuccessor：为迭代器优化的键截断方法。
// 设计目标：提高数据库的读写效率，确保排序规则一致。
// 4. 过滤策略 (InternalFilterPolicy)
// 包装用户定义的过滤器（如 Bloom Filter）。
// 作用：将内部键的过滤映射到用户键的过滤，避免误操作。
// 接口：
// CreateFilter：为用户键生成过滤器。
// KeyMayMatch：判断用户键是否可能匹配过滤器。
// 5. 辅助类 LookupKey
// 功能：封装用于内存表查找的键数据结构。
// 兼容 MemTable 的键格式。
// 可从中提取用户键或内部键。
// 内存优化：预分配固定大小的 char 数组 (space_) 以减少堆分配。
namespace leveldb {

// Grouping of constants.  We may want to make some of these
// parameters set via options.
namespace config {
static const int kNumLevels = 7;

// Level-0 compaction is started when we hit this many files.
static const int kL0_CompactionTrigger = 4;

// level0 文件的数量的松散限制, 我门在这个节点上降低写入
static const int kL0_SlowdownWritesTrigger = 8;

// Maximum number of level-0 files.  We stop writes at this point.
static const int kL0_StopWritesTrigger = 12;

// Maximum level to which a new compacted memtable is pushed if it
// does not create overlap.  We try to push to level 2 to avoid the
// relatively expensive level 0=>1 compactions and to avoid some
// expensive manifest file operations.  We do not push all the way to
// the largest level since that can generate a lot of wasted disk
// space if the same key space is being repeatedly overwritten.
static const int kMaxMemCompactLevel = 2;

// Approximate gap in bytes between samples of data read during iteration.
static const int kReadBytesPeriod = 1048576;

}  // namespace config

class InternalKey;

// Value types encoded as the last component of internal keys.
// DO NOT CHANGE THESE ENUM VALUES: they are embedded in the on-disk
// data structures.
enum ValueType { kTypeDeletion = 0x0, kTypeValue = 0x1 };
// kValueTypeForSeek defines the ValueType that should be passed when
// constructing a ParsedInternalKey object for seeking to a particular
// sequence number (since we sort sequence numbers in decreasing order
// and the value type is embedded as the low 8 bits in the sequence
// number in internal keys, we need to use the highest-numbered
// ValueType, not the lowest).
static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t SequenceNumber;

// We leave eight bits empty at the bottom so a type and sequence#
// can be packed together into 64-bits.
static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);

struct ParsedInternalKey {
  Slice user_key; // 用户键
  SequenceNumber sequence; // 序列号
  ValueType type; // 键的类型

  ParsedInternalKey() {}  // 默认初始化, 不进行任何初始化, 未初始化的变量可以快速创建
  ParsedInternalKey(const Slice& u, const SequenceNumber& seq, ValueType t)
      : user_key(u), sequence(seq), type(t) {}// 带参数的构造函数
  std::string DebugString() const; // 调试字符串
};

// Return the length of the encoding of "key".
inline size_t InternalKeyEncodingLength(const ParsedInternalKey& key) {
  return key.user_key.size() + 8;
}

// Append the serialization of "key" to *result.
void AppendInternalKey(std::string* result, const ParsedInternalKey& key);

// Attempt to parse an internal key from "internal_key".  On success,
// stores the parsed data in "*result", and returns true.
//
// On error, returns false, leaves "*result" in an undefined state.
bool ParseInternalKey(const Slice& internal_key, ParsedInternalKey* result);

// Returns the user key portion of an internal key.
inline Slice ExtractUserKey(const Slice& internal_key) {
  assert(internal_key.size() >= 8);
  return Slice(internal_key.data(), internal_key.size() - 8);
}

// A comparator for internal keys that uses a specified comparator for
// the user key portion and breaks ties by decreasing sequence number.
class InternalKeyComparator : public Comparator {
 private:
  const Comparator* user_comparator_;

 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) {}
  const char* Name() const override;
  int Compare(const Slice& a, const Slice& b) const override;
  void FindShortestSeparator(std::string* start,
                             const Slice& limit) const override;
  void FindShortSuccessor(std::string* key) const override;

  const Comparator* user_comparator() const { return user_comparator_; }

  int Compare(const InternalKey& a, const InternalKey& b) const;
};

// Filter policy wrapper that converts from internal keys to user keys
class InternalFilterPolicy : public FilterPolicy {
 private:
  const FilterPolicy* const user_policy_;

 public:
  explicit InternalFilterPolicy(const FilterPolicy* p) : user_policy_(p) {}
  const char* Name() const override;
  void CreateFilter(const Slice* keys, int n, std::string* dst) const override;
  bool KeyMayMatch(const Slice& key, const Slice& filter) const override;
};

// Modules in this directory should keep internal keys wrapped inside
// the following class instead of plain strings so that we do not
// incorrectly use string comparisons instead of an InternalKeyComparator.
class InternalKey {
 private:
  std::string rep_;

 public:
  InternalKey() {}  // Leave rep_ as empty to indicate it is invalid
  InternalKey(const Slice& user_key, SequenceNumber s, ValueType t) {
    AppendInternalKey(&rep_, ParsedInternalKey(user_key, s, t));
  }

  bool DecodeFrom(const Slice& s) {
    rep_.assign(s.data(), s.size());
    return !rep_.empty();
  }

  Slice Encode() const {
    assert(!rep_.empty());
    return rep_;
  }

  Slice user_key() const { return ExtractUserKey(rep_); }

  void SetFrom(const ParsedInternalKey& p) {
    rep_.clear();
    AppendInternalKey(&rep_, p);
  }

  void Clear() { rep_.clear(); }

  std::string DebugString() const;
};

inline int InternalKeyComparator::Compare(const InternalKey& a,
                                          const InternalKey& b) const {
  return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result) {
  const size_t n = internal_key.size();
  if (n < 8) return false;
  uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
  uint8_t c = num & 0xff;
  result->sequence = num >> 8;
  result->type = static_cast<ValueType>(c);
  result->user_key = Slice(internal_key.data(), n - 8);
  return (c <= static_cast<uint8_t>(kTypeValue));
}

// A helper class useful for DBImpl::Get()
// 为什么需要这个类?
class LookupKey {// 相当于 给一些信息 来辅助检索
 public:
  // Initialize *this for looking up user_key at a snapshot with
  // the specified sequence number.
  LookupKey(const Slice& user_key, SequenceNumber sequence);// 构造函数, 初始化 以查看指定 序列号的user_key

  LookupKey(const LookupKey&) = delete; 
  LookupKey& operator=(const LookupKey&) = delete;

  ~LookupKey();// 析构函数

  // 返回一个适合在 MemTable 中查找的键
  Slice memtable_key() const { return Slice(start_, end_ - start_); }

  // 返回一个内部键（适合传递给内部迭代器）
  Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

  // 返回用户键
  Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

 private:
  // We construct a char array of the form:
  //    klength  varint32               <-- start_
  //    userkey  char[klength]          <-- kstart_
  //    tag      uint64
  //                                    <-- end_
  // The array is a suitable MemTable key.
  // The suffix starting with "userkey" can be used as an InternalKey.
  const char* start_;
  const char* kstart_;
  const char* end_;
  char space_[200];  // Avoid allocation for short keys
};

inline LookupKey::~LookupKey() {
  if (start_ != space_) delete[] start_;
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_DBFORMAT_H_
