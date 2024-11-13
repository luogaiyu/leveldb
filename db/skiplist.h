// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
/**
 * 业务背景: 这个代码实现了完整的跳表数据结构
 */
#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
#define STORAGE_LEVELDB_DB_SKIPLIST_H_
/**
 * 对于C++来说 内联函数 模版或者小型项目 是可以将对应的实现逻辑写在代码中的
 */

// SkipList

// 主类，表示跳表的核心数据结构。
// 包含内部逻辑、节点管理和操作方法（如插入、搜索等）。
// Node

// 嵌套结构，表示跳表中的节点。
// 每个节点保存一个键（key）和多个指向下一节点的指针（next_）。
// Comparator

// 比较器类，用于比较两个键的大小关系。
// 以模板参数的形式传入跳表，支持自定义比较逻辑。
// Iterator

// 跳表的迭代器类，用于遍历跳表中的节点。
// 提供操作方法，如移动到下一个节点或第一个节点等。
// Arena

// 内存分配器，管理跳表的内存分配（例如节点的分配）。
// 通过 AllocateAligned 方法提供对齐的内存分配支持。
// Random

// 随机数生成器，用于生成节点的随机高度。
// 通过概率分布控制跳表的层级分布特性。


// 模板类的特殊性

// C++ 的模板类需要在编译时实例化，而模板的实现必须在编译器能够看到的地方。
// 因此，模板类通常将声明和实现都放在头文件中，而不像普通类那样分为 .h 和 .cpp 文件。
// 性能优化

// 跳表的实现中，许多函数（如 Iterator 方法和内联函数）被标记为 inline，建议编译器在调用时直接展开，减少函数调用开销。
// 为了支持 inline 优化，这些函数的定义也需要放在头文件中。
// 简化管理

// 头文件直接包含实现逻辑，便于模板类的使用者只通过头文件即可访问全部功能。
// 避免分散到多个文件中导致维护复杂性增加。

// 对外暴露的主要特征：
// 数据存储有序，支持自定义排序。
// 提供基本操作接口：Insert、Contains。
// 提供完整的遍历功能：Iterator。
// 支持多线程读无锁，写需要同步。
// 可定制比较器和内存管理方式。
// Thread safety
// -------------
//
// Writes require external synchronization, most likely a mutex.
// Reads require a guarantee that the SkipList will not be destroyed
// while the read is in progress.  Apart from that, reads progress
// without any internal locking or synchronization.
//
// Invariants:
//
// (1) Allocated nodes are never deleted until the SkipList is
// destroyed.  This is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) The contents of a Node except for the next/prev pointers are
// immutable after the Node has been linked into the SkipList.
// Only Insert() modifies the list, and it is careful to initialize
// a node and use release-stores to publish the nodes in one or
// more lists.
//
// ... prev vs. next pointer ordering ...

#include <atomic>
#include <cassert>
#include <cstdlib>

#include "util/arena.h"
#include "util/random.h"

namespace leveldb {

// 创建一个模版类, 类似Java中的范型
template <typename Key, class Comparator> 
class SkipList {
 private:
  struct Node;

 public:
  // 通过两个参数, 定义对象创建的方式
  explicit SkipList(Comparator cmp, Arena* arena);
  
  // 禁止拷贝构造函数 和 拷贝赋值函数 = delete是c++中的一种特殊用法
  SkipList(const SkipList&) = delete;
  SkipList& operator=(const SkipList&) = delete;

  // 将当前的key 插入到列表中
  // 要求: 当前列表中没有与当前的key相等的元素
  void Insert(const Key& key);
  // 判断当前的列表 是否包含当前的key
  bool Contains(const Key& key) const;

  // 迭代器: 可以遍历跳表的内容
  class Iterator {
   public:
    // 构造函数
    explicit Iterator(const SkipList* list);

  // 检查迭代器是否有效
    bool Valid() const;

     // 返回当前位置的键
    // REQUIRES: Valid()
    const Key& key() const;

     // 前进到下一个位置
    // REQUIRES: Valid()
    void Next();

     // 前进到前一个位置
    // REQUIRES: Valid()
    void Prev();

     // 前进到第一个键 >= target 的条目
    void Seek(const Key& target);

    // 定位到列表的第一个条目
    void SeekToFirst();

    // 定位到列表的最后一个条目
    void SeekToLast();

   private:
    const SkipList* list_;  // 指向 SkipList 的指针
    Node* node_;  // 指向当前节点的指针
     // 默认拷贝构造函数和拷贝赋值运算符是可用的
  };

 private:
  enum { kMaxHeight = 12 }; // 使用enum 会防止内存分配, todo
  // inline 建议编译器 在调用该函数的时候直接展开函数体
  // 内联函数: 能够降低
  inline int GetMaxHeight() const {
    // 通过原子的方式来取 max_height_ 的数值
    return max_height_.load(std::memory_order_relaxed);
  }

  // 创建一个新节点，包含给定的键和高度
  Node* NewNode(const Key& key, int height);
  
  // 生成一个随机的高度值
  int RandomHeight();
  
  // 判断两个键是否相等
  bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }

  // 判断给定的键是否大于节点 "n" 中存储的数据
  bool KeyIsAfterNode(const Key& key, Node* n) const;

  // 返回第一个键大于或等于给定键的节点
  // 如果没有这样的节点，则返回 nullptr
  // 如果 prev 非空，则填充 prev[level] 为 "level" 层的前一个节点的指针，对于 [0..max_height_-1] 中的每一层
  Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

  // 返回键小于给定键的最新节点
  // 如果没有这样的节点，则返回 head_
  Node* FindLessThan(const Key& key) const;

  // 返回列表中的最后一个节点
  // 如果列表为空，则返回 head_
  Node* FindLast() const;

  // 在构造后不可变
  Comparator const compare_; // 指针本身不可变
  Arena* const arena_;  // 用于节点分配的 Arena
  Node* const head_;// 指针本身不可变

  // 仅由 Insert() 修改。读者可以竞态读取，但过时的值是可以接受的
  std::atomic<int> max_height_;  // 整个列表的高度

  // 仅由 Insert() 读写
  Random rnd_;
};

// 接下来 是具体的实现细节:  嵌套结构体 Node 的实现
template <typename Key, class Comparator>
struct SkipList<Key, Comparator>::Node {

  explicit Node(const Key& k) : key(k) {}

  Key const key;

  // Accessors/mutators for links.  Wrapped in methods so we can
  // add the appropriate barriers as necessary.
  Node* Next(int n) {
    assert(n >= 0);
    // Use an 'acquire load' so that we observe a fully initialized
    // version of the returned Node.
    return next_[n].load(std::memory_order_acquire);
  }
  void SetNext(int n, Node* x) {
    assert(n >= 0);
    // Use a 'release store' so that anybody who reads through this
    // pointer observes a fully initialized version of the inserted node.
    next_[n].store(x, std::memory_order_release);
  }

  // No-barrier variants that can be safely used in a few locations.
  Node* NoBarrier_Next(int n) {
    assert(n >= 0);
    return next_[n].load(std::memory_order_relaxed);
  }
  void NoBarrier_SetNext(int n, Node* x) {
    assert(n >= 0);
    next_[n].store(x, std::memory_order_relaxed);
  }

 private:
  // Array of length equal to the node height.  next_[0] is lowest level link.
  std::atomic<Node*> next_[1];
};

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::NewNode(
    const Key& key, int height) {
  char* const node_memory = arena_->AllocateAligned(
      sizeof(Node) + sizeof(std::atomic<Node*>) * (height - 1));
  return new (node_memory) Node(key);
}

template <typename Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  node_ = nullptr;
}

template <typename Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::Valid() const {
  return node_ != nullptr;
}

template <typename Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Next() {
  assert(Valid());
  node_ = node_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Prev() {
  // Instead of using explicit "prev" links, we just search for the
  // last node that falls before key.
  assert(Valid());
  node_ = list_->FindLessThan(node_->key);
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, nullptr);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToFirst() {
  node_ = list_->head_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToLast() {
  node_ = list_->FindLast();
  if (node_ == list_->head_) {
    node_ = nullptr;
  }
}

template <typename Key, class Comparator>
int SkipList<Key, Comparator>::RandomHeight() {
  // Increase height with probability 1 in kBranching
  static const unsigned int kBranching = 4;
  int height = 1;
  while (height < kMaxHeight && rnd_.OneIn(kBranching)) {
    height++;
  }
  assert(height > 0);
  assert(height <= kMaxHeight);
  return height;
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::KeyIsAfterNode(const Key& key, Node* n) const {
  // null n is considered infinite
  return (n != nullptr) && (compare_(n->key, key) < 0);
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindGreaterOrEqual(const Key& key,
                                              Node** prev) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (KeyIsAfterNode(key, next)) {
      // Keep searching in this list
      x = next;
    } else {
      if (prev != nullptr) prev[level] = x;
      if (level == 0) {
        return next;
      } else {
        // Switch to next list
        level--;
      }
    }
  }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindLessThan(const Key& key) const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    assert(x == head_ || compare_(x->key, key) < 0);
    Node* next = x->Next(level);
    if (next == nullptr || compare_(next->key, key) >= 0) {
      if (level == 0) {
        return x;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindLast()
    const {
  Node* x = head_;
  int level = GetMaxHeight() - 1;
  while (true) {
    Node* next = x->Next(level);
    if (next == nullptr) {
      if (level == 0) {
        return x;
      } else {
        // Switch to next list
        level--;
      }
    } else {
      x = next;
    }
  }
}

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp),
      arena_(arena),
      head_(NewNode(0 /* any key will do */, kMaxHeight)),
      max_height_(1),
      rnd_(0xdeadbeef) {
  for (int i = 0; i < kMaxHeight; i++) {
    head_->SetNext(i, nullptr);
  }
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key& key) {
  // TODO(opt): We can use a barrier-free variant of FindGreaterOrEqual()
  // here since Insert() is externally synchronized.
  Node* prev[kMaxHeight];
  Node* x = FindGreaterOrEqual(key, prev);

  // Our data structure does not allow duplicate insertion
  assert(x == nullptr || !Equal(key, x->key));

  int height = RandomHeight();
  if (height > GetMaxHeight()) {
    for (int i = GetMaxHeight(); i < height; i++) {
      prev[i] = head_;
    }
    // It is ok to mutate max_height_ without any synchronization
    // with concurrent readers.  A concurrent reader that observes
    // the new value of max_height_ will see either the old value of
    // new level pointers from head_ (nullptr), or a new value set in
    // the loop below.  In the former case the reader will
    // immediately drop to the next level since nullptr sorts after all
    // keys.  In the latter case the reader will use the new node.
    max_height_.store(height, std::memory_order_relaxed);
  }

  x = NewNode(key, height);
  for (int i = 0; i < height; i++) {
    // NoBarrier_SetNext() suffices since we will add a barrier when
    // we publish a pointer to "x" in prev[i].
    x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
    prev[i]->SetNext(i, x);
  }
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::Contains(const Key& key) const {
  Node* x = FindGreaterOrEqual(key, nullptr);
  if (x != nullptr && Equal(key, x->key)) {
    return true;
  } else {
    return false;
  }
}

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_SKIPLIST_H_
