// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "helpers/memenv/memenv.h"

#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <vector>

#include "leveldb/env.h"
#include "leveldb/status.h"
#include "port/port.h"
#include "port/thread_annotations.h"
#include "util/mutexlock.h"
/**
 * 这段代码实现了一个完全在内存中运行的文件系统模拟 InMemoryEnv，用于测试和调试 LevelDB 的文件操作。通过 FileState 类管理文件状态，SequentialFileImpl、RandomAccessFileImpl 和 WritableFileImpl 类实现不同的文件操作接口，InMemoryEnv 类则实现了 Env 接口的方法，提供了一个完整的内存文件系统。
 */
namespace leveldb {

namespace {
/**
 * 
FileState 类表示一个文件的状态，包括文件的引用计数、大小和数据块。
Ref 和 Unref 方法用于增加和减少引用计数，当引用计数为零时删除对象。
Size 方法返回文件的大小。
Truncate 方法清空文件内容。
Read 方法从文件中读取数据。
Append 方法向文件中追加数据。
 */
class FileState {
 public:
  // FileStates are reference counted. The initial reference count is zero
  // and the caller must call Ref() at least once.
  FileState() : refs_(0), size_(0) {}

  /**
   * 不允许复制和赋值
   */
  FileState(const FileState&) = delete;
  FileState& operator=(const FileState&) = delete;

  // Increase the reference count.
  void Ref() { // 防止内存泄漏, 使用ref来进行进行管理引用
    MutexLock lock(&refs_mutex_);
    ++refs_;
  }

  /**
   * 相当于用这个方法做了 对象的内存的管理, 使用refs 来标记对象被引用的次数, 这个在Java中是JVM来进行管理,但是在C++中需要自己来做
   */
  void Unref() {
    bool do_delete = false;

    {
      MutexLock lock(&refs_mutex_);
      --refs_;
      assert(refs_ >= 0);
      if (refs_ <= 0) {
        do_delete = true;
      }
    }

    if (do_delete) {
      delete this;
    }
  }

  uint64_t Size() const {
    MutexLock lock(&blocks_mutex_);// 这个是个抢锁操作
    return size_;// 返回对应的结构体的大小
  }

  void Truncate() { // 作用: 用于清空某个数据结构的所有内容
    MutexLock lock(&blocks_mutex_);// 用于锁定 [blocks_mutex_] 这个是个互斥锁, 用于确保 多线程环境中 对共享内存的访问是安全的
    for (char*& block : blocks_) {// 然后删除 对应的 blocks_ 中的指针
      delete[] block;// 
    }
    blocks_.clear();// 然后清除对应的数据
    size_ = 0;
  }

//Read 方法 用于从文件中 读取数据, 根据给定的偏移量和读取长度, 从 blocks_ (预分配的块读取数据) 并把数据存储在提供的缓冲区 
  Status Read(uint64_t offset, size_t n, Slice* result, char* scratch) const { // 
    MutexLock lock(&blocks_mutex_);// 确保在多线程环境下 对共享资源安全
    // 检查给定的偏移量是否大于文件的总大小。如果是，则返回一个 IOError 状态。
    if (offset > size_) {
      return Status::IOError("Offset greater than file size.");
    }
    // 计算可用字节数
    const uint64_t available = size_ - offset;
    if (n > available) {
      n = static_cast<size_t>(available);
    }
    // 处理零字节读取
    if (n == 0) {
      *result = Slice();
      return Status::OK();
    }
    // 断言检查
    assert(offset / kBlockSize <= std::numeric_limits<size_t>::max());

    // static_cast<size_t> 使用 强制转换
    size_t block = static_cast<size_t>(offset / kBlockSize); // block 号
    //block的 偏移量
    size_t block_offset = offset % kBlockSize;
    // 计算要复制的字节数
    size_t bytes_to_copy = n;
    char* dst = scratch;// 指向目标缓存区

    while (bytes_to_copy > 0) {// 将数据 从block中读取出来, 相当于在磁盘上读了一些字节, 本质的操作是 std::memcpy
      size_t avail = kBlockSize - block_offset;
      if (avail > bytes_to_copy) {
        avail = bytes_to_copy;
      }
      std::memcpy(dst, blocks_[block] + block_offset, avail); // 复制数据....

      bytes_to_copy -= avail;
      dst += avail;
      block++;
      block_offset = 0;
    }

    *result = Slice(scratch, n); // 相当于 从stratch 缓冲区, 读取 n个字节的数据, 并作为Slice 作为result的结果
    return Status::OK();// 返回 工作正常的状态
  }
/**
 * RAII（Resource Acquisition Is Initialization，资源获取即初始化）是一种编程技术，
 * 主要用于管理程序中的资源，如内存、文件句柄、网络连接等。
 * 这种技术的核心思想是在对象创建时获取资源，并在对象销毁时释放资源。通过这种方式，
 * 可以确保资源在不再需要时能够被自动且及时地释放，从而避免资源泄漏。
 */
  Status Append(const Slice& data) {
    // 
    const char* src = data.data();// 获取 data的数据指针
    size_t src_len = data.size(); // 获取 data的数据长度

    MutexLock lock(&blocks_mutex_);// 接着 对 这个操作上锁
    while (src_len > 0) {
      size_t avail; // 声明一个变量来存放 剩余
      size_t offset = size_ % kBlockSize;
      /**
       * 查看当前 是否还有空间
       * 查看当前文件的大小
       */
      if (offset != 0) {
        // There is some room in the last block.
        avail = kBlockSize - offset;
      } else {
        // No room in the last block; push new one.
        blocks_.push_back(new char[kBlockSize]);
        avail = kBlockSize;
      }
      // 如果剩余空间大于文件长度, 那么将 可用空间转换为 文件剩余长度
      if (avail > src_len) {
        avail = src_len;
      }
      std::memcpy(blocks_.back() + offset, src, avail);// 进行内存复制
      src_len -= avail;
      src += avail;
      size_ += avail;
    }

    return Status::OK();
  }

 private:
  enum { kBlockSize = 8 * 1024 };

  // Private since only Unref() should be used to delete it.
  ~FileState() { Truncate(); }

  port::Mutex refs_mutex_;// 创建一个多线程的操作符, 是一个互斥类的锁, 用于保护共享资源
  int refs_ GUARDED_BY(refs_mutex_);// refs 表示引用次数
  /**
   * grarded_by : 相当于 在代码上 做了个标记, 但并不是实际的代码
   * 能够在文档生成时做标记
   */

  mutable port::Mutex blocks_mutex_;
  std::vector<char*> blocks_ GUARDED_BY(blocks_mutex_);
  uint64_t size_ GUARDED_BY(blocks_mutex_); // size_表示 文件的大小
};
/**
 * 
 * SequentialFileImpl 实现了 SequentialFile 接口，用于顺序读取文件。
 * RandomAccessFileImpl 实现了 RandomAccessFile 接口，用于随机访问文件。
 * WritableFileImpl 实现了 WritableFile 接口，用于写入文件。
 */
class SequentialFileImpl : public SequentialFile {// SequentialFileImpl 这个其实就是 接口定义模型, 实现放在实现类, 这样能够实现 对外暴露功能和具体逻辑的解耦
 public:
  explicit SequentialFileImpl(FileState* file) : file_(file), pos_(0) {
    file_->Ref();// 如果要是创建 就ref + 1 
  }

  ~SequentialFileImpl() override { file_->Unref(); }

  Status Read(size_t n, Slice* result, char* scratch) override {
    Status s = file_->Read(pos_, n, result, scratch);// 使用pos 顺序读取
    if (s.ok()) {
      pos_ += result->size();// 当前的指针 来记录 文本的
    }
    return s;
  }

  Status Skip(uint64_t n) override {// 跳过
    if (pos_ > file_->Size()) {
      return Status::IOError("pos_ > file_->Size()");
    }
    const uint64_t available = file_->Size() - pos_;// 可用位置
    if (n > available) {
      n = available;
    }
    pos_ += n;
    return Status::OK();
  }

 private:
  FileState* file_;// 主要定义一个文件
  uint64_t pos_;// 这个pos 指的就是 对应字节数量
};

class RandomAccessFileImpl : public RandomAccessFile {// 提供
 public:
  explicit RandomAccessFileImpl(FileState* file) : file_(file) { file_->Ref(); }

  ~RandomAccessFileImpl() override { file_->Unref(); }

  Status Read(uint64_t offset, size_t n, Slice* result,
              char* scratch) const override {
    return file_->Read(offset, n, result, scratch);// 在任意位置进行读取
  }

 private:
  FileState* file_;
};

class WritableFileImpl : public WritableFile {// WritableFile 的实现类
 public:
  WritableFileImpl(FileState* file) : file_(file) { file_->Ref(); }

  ~WritableFileImpl() override { file_->Unref(); }

  Status Append(const Slice& data) override { return file_->Append(data); }

  Status Close() override { return Status::OK(); }// 可能在其他地方已经实现了这个Close()
  Status Flush() override { return Status::OK(); }
  Status Sync() override { return Status::OK(); }

 private:
  FileState* file_;
};
// NoOpLogger 是一个空的日志记录器，不执行任何操作。
class NoOpLogger : public Logger {
 public:
  void Logv(const char* format, std::va_list ap) override {}
};
/**
 * 
InMemoryEnv 类继承自 EnvWrapper，实现了 Env 接口的方法。
NewSequentialFile、NewRandomAccessFile、NewWritableFile 和 NewAppendableFile 方法分别创建不同类型的文件对象。
FileExists 方法检查文件是否存在。
GetChildren 方法获取指定目录下的文件列表。
RemoveFile 方法删除文件。
CreateDir 和 RemoveDir 方法是空操作，因为内存环境不支持目录操作。
GetFileSize 方法获取文件的大小。
RenameFile 方法重命名文件。
LockFile 和 UnlockFile 方法是空操作，因为内存环境不支持文件锁。
GetTestDirectory 方法返回一个测试目录。
NewLogger 方法创建一个空的日志记录器。
 */
class InMemoryEnv : public EnvWrapper {// EnvWrapper 通常是一个类
 public:
  explicit InMemoryEnv(Env* base_env) : EnvWrapper(base_env) {}

  ~InMemoryEnv() override {// 通过ref 来控制 内存
    for (const auto& kvp : file_map_) {
      kvp.second->Unref();
    }
  }

  // Partial implementation of the Env interface.
  // 用于创建一个新的 SequentialFile 实例
  Status NewSequentialFile(const std::string& fname,
                           SequentialFile** result) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = nullptr;
      return Status::IOError(fname, "File not found");
    }

    *result = new SequentialFileImpl(file_map_[fname]);// 通过 文件的指针 
    return Status::OK();
  }

  Status NewRandomAccessFile(const std::string& fname,
                             RandomAccessFile** result) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      *result = nullptr;
      return Status::IOError(fname, "File not found");
    }

    *result = new RandomAccessFileImpl(file_map_[fname]);
    return Status::OK();
  }

  Status NewWritableFile(const std::string& fname,
                         WritableFile** result) override {
    MutexLock lock(&mutex_);
    FileSystem::iterator it = file_map_.find(fname);

    FileState* file;
    if (it == file_map_.end()) {
      // File is not currently open.
      file = new FileState();
      file->Ref();
      file_map_[fname] = file;
    } else {
      file = it->second;
      file->Truncate();
    }

    *result = new WritableFileImpl(file);
    return Status::OK();
  }

  Status NewAppendableFile(const std::string& fname,
                           WritableFile** result) override {// 使用 添加文件 append
    MutexLock lock(&mutex_);
    FileState** sptr = &file_map_[fname];
    FileState* file = *sptr;
    if (file == nullptr) {
      file = new FileState();
      file->Ref();
    }
    *result = new WritableFileImpl(file);
    return Status::OK();
  }

  bool FileExists(const std::string& fname) override {
    MutexLock lock(&mutex_);
    return file_map_.find(fname) != file_map_.end();
  }

  Status GetChildren(const std::string& dir,
                     std::vector<std::string>* result) override {
    MutexLock lock(&mutex_);
    result->clear();

    for (const auto& kvp : file_map_) {
      const std::string& filename = kvp.first; // 

      if (filename.size() >= dir.size() + 1 && filename[dir.size()] == '/' &&
          Slice(filename).starts_with(Slice(dir))) {
        result->push_back(filename.substr(dir.size() + 1));
      }
    }

    return Status::OK();
  }

  void RemoveFileInternal(const std::string& fname)
      EXCLUSIVE_LOCKS_REQUIRED(mutex_) {
    if (file_map_.find(fname) == file_map_.end()) {
      return;
    }

    file_map_[fname]->Unref();
    file_map_.erase(fname);
  }

  Status RemoveFile(const std::string& fname) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return Status::IOError(fname, "File not found");
    }

    RemoveFileInternal(fname);// 内部删除文件, 在文件系统中 取消这个文件的注册
    return Status::OK();
  }

  Status CreateDir(const std::string& dirname) override { return Status::OK(); }// 创建目录

  Status RemoveDir(const std::string& dirname) override { return Status::OK(); }// 去除目录

  Status GetFileSize(const std::string& fname, uint64_t* file_size) override {// 获取文件大小
    MutexLock lock(&mutex_);
    if (file_map_.find(fname) == file_map_.end()) {
      return Status::IOError(fname, "File not found");
    }

    *file_size = file_map_[fname]->Size();
    return Status::OK();
  }

  Status RenameFile(const std::string& src,
                    const std::string& target) override {
    MutexLock lock(&mutex_);
    if (file_map_.find(src) == file_map_.end()) {
      return Status::IOError(src, "File not found");
    }

    RemoveFileInternal(target);
    file_map_[target] = file_map_[src];
    file_map_.erase(src);
    return Status::OK();
  }

  Status LockFile(const std::string& fname, FileLock** lock) override {
    *lock = new FileLock;
    return Status::OK();
  }

  Status UnlockFile(FileLock* lock) override {
    delete lock;
    return Status::OK();
  }

  Status GetTestDirectory(std::string* path) override {
    *path = "/test";
    return Status::OK();
  }

  Status NewLogger(const std::string& fname, Logger** result) override {
    *result = new NoOpLogger;
    return Status::OK();
  }

 private:
  // Map from filenames to FileState objects, representing a simple file system.
  typedef std::map<std::string, FileState*> FileSystem; //设置对应的类型 FileSystem

  port::Mutex mutex_;
  FileSystem file_map_ GUARDED_BY(mutex_);// FileSystem: 使用 文件系统
};

}  // namespace

Env* NewMemEnv(Env* base_env) { return new InMemoryEnv(base_env); }

}  // namespace leveldb
