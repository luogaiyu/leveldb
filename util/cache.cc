// 引入所需的头文件, 包括LevelDB 的缓存模块
#include "leveldb/cache.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>

#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/hash.h"
#include "util/mutexlock.h"

namespace leveldb {// 定义命名空间

Cache::~Cache() {}// 确保派生类的析构函数

namespace {

struct LRUHandle {
  void* value;                               // 缓存条目的值 - void* 指的是通用类型, 实际上存的是地址, 能够指向不同地方
  void (*deleter)(const Slice&, void* value);// 当条目被移除时调用的删除函数
  LRUHandle* next_hash;                      // 指向下一个具有相同hash值的LRUHandle, 用于处理hash冲突
  LRUHandle* next;                           // 指向链表中下一个LRUHandle 用于维护LRU链表的顺序
  LRUHandle* prev;                           // 指向上一个 LRUHandle 用于维护LRU链表的顺序
  size_t charge;                             // 通常用于计算缓存的总大小。 TODO(opt): Only allow uint32_t?
  size_t key_length;                         //存储键的大小
  bool in_cache;                             // 是否在缓存中出现, hether entry is in the cache.
  uint32_t refs;                             // 表示该条目的引用次数, 包括 缓存本身的引用次数, 如果引用次数为0的时候, 条目可以被安全的删除, References, including cache reference, if present.
  uint32_t hash;                             // 存储键的hash值, 用于快速分片和比较 Hash of key(); used for fast sharding and comparisons
  char key_data[1];                          // 用于存储键的实际内容, 允许在 结构体末尾动态分配内存来存储键 Beginning of key

  Slice key() const {
    // next is only equal to this if the LRU handle is the list head of an
    // empty list. List heads never have meaningful keys.
    assert(next != this);

    return Slice(key_data, key_length);//Slice(const char* d, size_t n) : data_(d), size_(n) {} Slice(const char* s) : data_(s), size_(strlen(s)) {} 的区别是什么? 我想知道 是不是相当于会少一次 strlen的函数调用, 我不太理解 为什么不直接用 Slice(const char* s)
  }
};
// 相当于给 LRUHandle 新增 hash表管理
class HandleTable { 
 public:
  HandleTable() : length_(0), elems_(0), list_(nullptr) { Resize(); }// 构建方法
  ~HandleTable() { delete[] list_; }// 删除方法

  LRUHandle* Lookup(const Slice& key, uint32_t hash) {// 通过对应的key和hash来进行检索, 找到对应的 LRUHandle 指针
    return *FindPointer(key, hash);
  }

// 插入一个新的 LRUHandle, 
  LRUHandle* Insert(LRUHandle* h) {
    LRUHandle** ptr = FindPointer(h->key(), h->hash);
    LRUHandle* old = *ptr;// 这里指向的是 LRU 
    h->next_hash = (old == nullptr ? nullptr : old->next_hash);
    *ptr = h;
    if (old == nullptr) {
      ++elems_;
      if (elems_ > length_) {
        Resize();// 如果元素数量 超过当前的hash表的个数, 扩大当前的hash表数量
      }
    }
    return old;
  }
// 
  LRUHandle* Remove(const Slice& key, uint32_t hash) {
    LRUHandle** ptr = FindPointer(key, hash);
    LRUHandle* result = *ptr;
    if (result != nullptr) {
      *ptr = result->next_hash;// 这一行代码 更新的是指针, 不需要再去对 前一个节点的 next_hash 做操作了
      --elems_;
    }
    return result;
  }

 private:
  uint32_t length_;
  uint32_t elems_;
  LRUHandle** list_;// 默认创建的是一个二维数组

  LRUHandle** FindPointer(const Slice& key, uint32_t hash) {// 哈希表中找到与给定键和哈希值匹配的 LRUHandle 对象, 如果没有的话, 返回最后一个
    LRUHandle** ptr = &list_[hash & (length_ - 1)];         // 计算 hash值 对应的桶索引, 首先取到对应的桶的索引
    while (*ptr != nullptr && ((*ptr)->hash != hash || key != (*ptr)->key())) {// 对当前 桶部分的索引进行遍历
      ptr = &(*ptr)->next_hash;
    }
    return ptr;
  }

  void Resize() {
    uint32_t new_length = 4;
    while (new_length < elems_) {
      new_length *= 2;
    }
    LRUHandle** new_list = new LRUHandle*[new_length];
    memset(new_list, 0, sizeof(new_list[0]) * new_length);// 扩充数组
    uint32_t count = 0;
    for (uint32_t i = 0; i < length_; i++) {
      LRUHandle* h = list_[i];
      while (h != nullptr) {
        LRUHandle* next = h->next_hash;
        uint32_t hash = h->hash;
        LRUHandle** ptr = &new_list[hash & (new_length - 1)];
        h->next_hash = *ptr;
        *ptr = h;
        h = next;
        count++;
      }
    }
    assert(elems_ == count);
    delete[] list_;
    list_ = new_list;
    length_ = new_length;
  }
};


/**
 * 单独的 共享缓存的 分片
 */
class LRUCache {
 public:
  LRUCache();  // 默认构造函数
  ~LRUCache(); // 默认 析构函数

  void SetCapacity(size_t capacity) { capacity_ = capacity; } // 设置容量的方法
  Cache::Handle* Insert(const Slice& key, uint32_t hash, void* value,
                        size_t charge,
                        void (*deleter)(const Slice& key, void* value)); // 插入方法
  Cache::Handle* Lookup(const Slice& key, uint32_t hash);// 查找方法 等等
  void Release(Cache::Handle* handle); // 释放LRU对象
  void Erase(const Slice& key, uint32_t hash);// 擦除 对应的key 和 hash
  void Prune(); // 剪枝操作
  size_t TotalCharge() const {// 总共申请的内存
    MutexLock l(&mutex_);
    return usage_;
  }

 private:
  void LRU_Remove(LRUHandle* e);
  void LRU_Append(LRUHandle* list, LRUHandle* e);
  void Ref(LRUHandle* e);
  void Unref(LRUHandle* e);
  bool FinishErase(LRUHandle* e) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

  size_t capacity_; // 容量

  mutable port::Mutex mutex_;// 多线程操作符, 防止出现多线程异常
  size_t usage_ GUARDED_BY(mutex_);// 容量

  // lru 列表的虚拟头节点
  // lru.prev: 最新条目, lru.next 最旧的条目
  // 注意: 每个元素都是 refs ==1和 is_cache 为true
  LRUHandle lru_ GUARDED_BY(mutex_);

  // 主要是已经被使用中的头节点
  // 每个元素 都是 refs 超过2 并且is_cache 这个属性为true
  LRUHandle in_use_ GUARDED_BY(mutex_);

  // 
  HandleTable table_ GUARDED_BY(mutex_);
};
//定义了一个类 LRUCache，实现了 LRU 缓存。包含插入、查找、释放、删除、修剪和获取总权重的方法。

LRUCache::LRUCache() : capacity_(0), usage_(0) {
  // Make empty circular linked lists.
  lru_.next = &lru_;
  lru_.prev = &lru_;
  in_use_.next = &in_use_;
  in_use_.prev = &in_use_;
}
// 构造函数初始化缓存，析构函数确保释放所有未释放的项。


LRUCache::~LRUCache() {
  assert(in_use_.next == &in_use_);  // Error if caller has an unreleased handle
  for (LRUHandle* e = lru_.next; e != &lru_;) {
    LRUHandle* next = e->next;
    assert(e->in_cache);
    e->in_cache = false;
    assert(e->refs == 1);  // Invariant of lru_ list.
    Unref(e);
    e = next;
  }
}

void LRUCache::Ref(LRUHandle* e) {
  if (e->refs == 1 && e->in_cache) {  // If on lru_ list, move to in_use_ list.
    LRU_Remove(e);
    LRU_Append(&in_use_, e);
  }
  e->refs++;
}

void LRUCache::Unref(LRUHandle* e) {
  assert(e->refs > 0);
  e->refs--;
  if (e->refs == 0) {  // Deallocate.
    assert(!e->in_cache);// 判断 当前的元素 是否在缓存中
    (*e->deleter)(e->key(), e->value);
    free(e);
  } else if (e->in_cache && e->refs == 1) {
    LRU_Remove(e);// 删除当前的元素
    LRU_Append(&lru_, e); 
  }
}

void LRUCache::LRU_Remove(LRUHandle* e) { // 将e这个节点删除 
  e->next->prev = e->prev;
  e->prev->next = e->next;
}

void LRUCache::LRU_Append(LRUHandle* list, LRUHandle* e) {// 将 e 这个节点添加到 list
  e->next = list;
  e->prev = list->prev;
  e->prev->next = e;
  e->next->prev = e;
}

Cache::Handle* LRUCache::Lookup(const Slice& key, uint32_t hash) {// 查找当前的元素
  MutexLock l(&mutex_);// 拿到多线程的操作符
  LRUHandle* e = table_.Lookup(key, hash);
  if (e != nullptr) {
    Ref(e);
  }
  return reinterpret_cast<Cache::Handle*>(e);// reinterpret_cast: 强制转化类型
}

void LRUCache::Release(Cache::Handle* handle) {// 释放元素
  MutexLock l(&mutex_);
  Unref(reinterpret_cast<LRUHandle*>(handle));
}

Cache::Handle* LRUCache::Insert(const Slice& key, uint32_t hash, void* value,
                                size_t charge,
                                void (*deleter)(const Slice& key,
                                                void* value)) {
  MutexLock l(&mutex_);

  LRUHandle* e =
      reinterpret_cast<LRUHandle*>(malloc(sizeof(LRUHandle) - 1 + key.size()));
  e->value = value;
  e->deleter = deleter;
  e->charge = charge;
  e->key_length = key.size();
  e->hash = hash;
  e->in_cache = false;
  e->refs = 1;  // for the returned handle.
  std::memcpy(e->key_data, key.data(), key.size());

  if (capacity_ > 0) {// 表示 缓存有容量
    e->refs++;  // for the cache's reference. 增加引用计数 refs, 表示缓存本身的引用
    e->in_cache = true; //
    LRU_Append(&in_use_, e);
    usage_ += charge;
    FinishErase(table_.Insert(e));
  } else {  // don't cache. (capacity_==0 is supported and turns off caching.)
    // next is read by key() in an assert, so it must be initialized
    e->next = nullptr; // 如果缓存是0 就不启用缓存, 将next 设置为 nullptr
  }
  while (usage_ > capacity_ && lru_.next != &lru_) {
    LRUHandle* old = lru_.next;
    assert(old->refs == 1);
    bool erased = FinishErase(table_.Remove(old->key(), old->hash));// 如果引用计数 1 表示只被 缓存引用, 移除old 条目
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }

  return reinterpret_cast<Cache::Handle*>(e);// 转换为 句柄, 句柄的含义就是 对资源或者其他对象的一种操作符
}

// If e != nullptr, finish removing *e from the cache; it has already been
// removed from the hash table.  Return whether e != nullptr.
bool LRUCache::FinishErase(LRUHandle* e) {// 负责从缓存中完全移除一个条目
  if (e != nullptr) {
    assert(e->in_cache);
    LRU_Remove(e);
    e->in_cache = false;
    usage_ -= e->charge;
    Unref(e);
  }
  return e != nullptr;
}

void LRUCache::Erase(const Slice& key, uint32_t hash) {
  MutexLock l(&mutex_);
  FinishErase(table_.Remove(key, hash));
}

void LRUCache::Prune() {
  MutexLock l(&mutex_);
  while (lru_.next != &lru_) {
    LRUHandle* e = lru_.next;
    assert(e->refs == 1);
    bool erased = FinishErase(table_.Remove(e->key(), e->hash));
    if (!erased) {  // to avoid unused variable when compiled NDEBUG
      assert(erased);
    }
  }
}

static const int kNumShardBits = 4;
static const int kNumShards = 1 << kNumShardBits;
/**
 * 目的是 提供并发能力, 将缓存分成多个独立的分片
 * 这一段的本质是实现并发能力-
 */
class ShardedLRUCache : public Cache {// 用于实现分片的LRU缓存
 private:
  LRUCache shard_[kNumShards]; // 创建 LRU的数组
  port::Mutex id_mutex_;       // 提供并发操作符, 本质上是使用 互斥锁 和 条件变量 这两个方法进行 
  uint64_t last_id_;           // last_id 指的是什么?

  static inline uint32_t HashSlice(const Slice& s) {// 使用Hash函数 对key进行hash,用来对数据进行分散
    return Hash(s.data(), s.size(), 0);
  }

  static uint32_t Shard(uint32_t hash) { return hash >> (32 - kNumShardBits); }// static 表示函数是静态的, 只在当前文件中

 public:
  explicit ShardedLRUCache(size_t capacity) : last_id_(0) {
    const size_t per_shard = (capacity + (kNumShards - 1)) / kNumShards;
    for (int s = 0; s < kNumShards; s++) {
      shard_[s].SetCapacity(per_shard);// 将每个分片 设置为 per_shard
    }
  }
  ~ShardedLRUCache() override {} // 析构 函数 插入 只会 对当前的LRUCache 进行操作
  Handle* Insert(const Slice& key, void* value, size_t charge,
                 void (*deleter)(const Slice& key, void* value)) override {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Insert(key, hash, value, charge, deleter);
  }
  Handle* Lookup(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    return shard_[Shard(hash)].Lookup(key, hash);// 使用 hash后的key 对 lru 进行检索
  }
  void Release(Handle* handle) override {
    LRUHandle* h = reinterpret_cast<LRUHandle*>(handle);// reinterpret_cast 不进行类型安全检查，因此使用时必须非常小心，确保转换是合理的
    shard_[Shard(h->hash)].Release(handle);
  }
  void Erase(const Slice& key) override {
    const uint32_t hash = HashSlice(key);
    shard_[Shard(hash)].Erase(key, hash);
  }
  void* Value(Handle* handle) override {
    return reinterpret_cast<LRUHandle*>(handle)->value;
  }
  uint64_t NewId() override {
    MutexLock l(&id_mutex_);
    return ++(last_id_);
  }
  void Prune() override {
    for (int s = 0; s < kNumShards; s++) {
      shard_[s].Prune();
    }
  }
  size_t TotalCharge() const override {
    size_t total = 0;
    for (int s = 0; s < kNumShards; s++) {
      total += shard_[s].TotalCharge();
    }
    return total;
  }
};
//定义了一个函数 NewLRUCache，用于创建一个新的 ShardedLRUCache 实例。

}  // end anonymous namespace

Cache* NewLRUCache(size_t capacity) { return new ShardedLRUCache(capacity); }// 对外能提供一个LRU的内存

}  // namespace leveldb
