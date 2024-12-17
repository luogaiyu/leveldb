// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// An Env is an interface used by the leveldb implementation to access
// operating system functionality like the filesystem etc.  Callers
// may wish to provide a custom Env object when opening a database to
// get fine gain control; e.g., to rate limit file system operations.
//
// All Env implementations are safe for concurrent access from
// multiple threads without any external synchronization.

#ifndef STORAGE_LEVELDB_INCLUDE_ENV_H_
#define STORAGE_LEVELDB_INCLUDE_ENV_H_

#include <cstdarg>
#include <cstdint>
#include <string>
#include <vector>

#include "leveldb/export.h"
#include "leveldb/status.h"

// This workaround can be removed when leveldb::Env::DeleteFile is removed.
#if defined(_WIN32)
// On Windows, the method name DeleteFile (below) introduces the risk of
// triggering undefined behavior by exposing the compiler to different
// declarations of the Env class in different translation units.
//
// This is because <windows.h>, a fairly popular header file for Windows
// applications, defines a DeleteFile macro. So, files that include the Windows
// header before this header will contain an altered Env declaration.
//
// This workaround ensures that the compiler sees the same Env declaration,
// independently of whether <windows.h> was included.
#if defined(DeleteFile)
#undef DeleteFile
#define LEVELDB_DELETEFILE_UNDEFINED
#endif  // defined(DeleteFile)
#endif  // defined(_WIN32)
/**
 * 
1. Env 类简介
Env 是一个操作系统抽象接口，允许用户通过自定义实现来替代默认环境，提供对底层系统资源的访问和控制。

特点：
所有实现都需要支持多线程并发访问。
提供默认实现 Env::Default()，适用于一般环境。
支持覆盖部分功能，通过 EnvWrapper 类代理调用目标 Env 实现。
 */
namespace leveldb {
// 使用前置声明 来减少编译时间 和编译错误
// 前置声明的作用是 减少 编译依赖, 2. 提高代码组织性 3. 避免循环依赖
class FileLock;// 用于文件锁定, 防止多个进程同时访问同一个文件
class Logger; // 用于日志记录, 记录系统运行时的信息
class RandomAccessFile; // 用于任意访问文件
class SequentialFile; // 用于顺序读取文件
class Slice;// 用于表示字符串片段, 常用于高效的数据处理
class WritableFile;// 用于写入文件, 可以创建新文件或追加到现有文件

class LEVELDB_EXPORT Env {
 public:
  Env();
/**
 * 禁用构造函数和赋值操作符 的原因
 * 1. 保持对象的唯一性
 * 2. 避免浅拷贝带来的问题, 避免 双重删除或悬空指针, 双重删除: 相当于 对某个指针指向的区域进行两次删除, 悬空指针 指的是 会导致未定义行为, 会导致程序崩溃或数据损坏
 * 3. 本质上就是单例模式的设计模式
 */
  Env(const Env&) = delete;// 禁用拷贝构造函数 todo: 这里还需要check下
  Env& operator=(const Env&) = delete;// 禁用 赋值操作符

  virtual ~Env();// 使用虚函数关键字 相当于有一个钩子, 能够在删除 基类的时候, 调用后续基于实现类的方法来一起删除, 如果不这么声明的话, 就可能只会调用基类的析构函数, 导致实现类会有一些资源没有被释放

  // Return a default environment suitable for the current operating
  // system.  Sophisticated users may wish to provide their own Env
  // implementation instead of relying on this default environment.
  //
  // The result of Default() belongs to leveldb and must never be deleted.
  static Env* Default();// static 通过类名可以直接调用这个静态的方法

  // Create an object that sequentially reads the file with the specified name.
  // On success, stores a pointer to the new file in *result and returns OK.
  // On failure stores nullptr in *result and returns non-OK.  If the file does
  // not exist, returns a non-OK status.  Implementations should return a
  // NotFound status when the file does not exist.
  //
  // The returned file will only be accessed by one thread at a time.
  // =0: 表示这个函数是纯虚函数
  // SequentialFile: 表示这个是一个指向指针的指针
  virtual Status NewSequentialFile(const std::string& fname,
                                   SequentialFile** result) = 0;

  // Create an object supporting random-access reads from the file with the
  // specified name.  On success, stores a pointer to the new file in
  // *result and returns OK.  On failure stores nullptr in *result and
  // returns non-OK.  If the file does not exist, returns a non-OK
  // status.  Implementations should return a NotFound status when the file does
  // not exist.
  //
  // The returned file may be concurrently accessed by multiple threads.
  virtual Status NewRandomAccessFile(const std::string& fname,
                                     RandomAccessFile** result) = 0;

  // Create an object that writes to a new file with the specified
  // name.  Deletes any existing file with the same name and creates a
  // new file.  On success, stores a pointer to the new file in
  // *result and returns OK.  On failure stores nullptr in *result and
  // returns non-OK.
  //
  // The returned file will only be accessed by one thread at a time.
  virtual Status NewWritableFile(const std::string& fname,
                                 WritableFile** result) = 0;

  // Create an object that either appends to an existing file, or
  // writes to a new file (if the file does not exist to begin with).
  // On success, stores a pointer to the new file in *result and
  // returns OK.  On failure stores nullptr in *result and returns
  // non-OK.
  //
  // The returned file will only be accessed by one thread at a time.
  //
  // May return an IsNotSupportedError error if this Env does
  // not allow appending to an existing file.  Users of Env (including
  // the leveldb implementation) must be prepared to deal with
  // an Env that does not support appending.
  virtual Status NewAppendableFile(const std::string& fname,
                                   WritableFile** result);

  // Returns true iff the named file exists.
  virtual bool FileExists(const std::string& fname) = 0;

  // Store in *result the names of the children of the specified directory.
  // The names are relative to "dir".
  // Original contents of *results are dropped.
  virtual Status GetChildren(const std::string& dir,
                             std::vector<std::string>* result) = 0;
  // Delete the named file.
  //
  // The default implementation calls DeleteFile, to support legacy Env
  // implementations. Updated Env implementations must override RemoveFile and
  // ignore the existence of DeleteFile. Updated code calling into the Env API
  // must call RemoveFile instead of DeleteFile.
  //
  // A future release will remove DeleteDir and the default implementation of
  // RemoveDir.
  virtual Status RemoveFile(const std::string& fname);

  // DEPRECATED: Modern Env implementations should override RemoveFile instead.
  //
  // The default implementation calls RemoveFile, to support legacy Env user
  // code that calls this method on modern Env implementations. Modern Env user
  // code should call RemoveFile.
  //
  // A future release will remove this method.
  virtual Status DeleteFile(const std::string& fname);

  // Create the specified directory.

  // =0: 标识这个函数是纯虚数函数 本质上就是抽象类中的方法
  virtual Status CreateDir(const std::string& dirname) = 0;

  // Delete the specified directory.
  //
  // The default implementation calls DeleteDir, to support legacy Env
  // implementations. Updated Env implementations must override RemoveDir and
  // ignore the existence of DeleteDir. Modern code calling into the Env API
  // must call RemoveDir instead of DeleteDir.
  //
  // A future release will remove DeleteDir and the default implementation of
  // RemoveDir.
  virtual Status RemoveDir(const std::string& dirname);

  // DEPRECATED: Modern Env implementations should override RemoveDir instead.
  //
  // The default implementation calls RemoveDir, to support legacy Env user
  // code that calls this method on modern Env implementations. Modern Env user
  // code should call RemoveDir.
  //
  // A future release will remove this method.
  virtual Status DeleteDir(const std::string& dirname);

  // Store the size of fname in *file_size.
  virtual Status GetFileSize(const std::string& fname, uint64_t* file_size) = 0;

  // Rename file src to target.
  virtual Status RenameFile(const std::string& src,
                            const std::string& target) = 0;

  // Lock the specified file.  Used to prevent concurrent access to
  // the same db by multiple processes.  On failure, stores nullptr in
  // *lock and returns non-OK.
  //
  // On success, stores a pointer to the object that represents the
  // acquired lock in *lock and returns OK.  The caller should call
  // UnlockFile(*lock) to release the lock.  If the process exits,
  // the lock will be automatically released.
  //
  // If somebody else already holds the lock, finishes immediately
  // with a failure.  I.e., this call does not wait for existing locks
  // to go away.
  //
  // May create the named file if it does not already exist.
  virtual Status LockFile(const std::string& fname, FileLock** lock) = 0;

  // Release the lock acquired by a previous successful call to LockFile.
  // REQUIRES: lock was returned by a successful LockFile() call
  // REQUIRES: lock has not already been unlocked.
  virtual Status UnlockFile(FileLock* lock) = 0;

  // Arrange to run "(*function)(arg)" once in a background thread.
  //
  // "function" may run in an unspecified thread.  Multiple functions
  // added to the same Env may run concurrently in different threads.
  // I.e., the caller may not assume that background work items are
  // serialized.
  virtual void Schedule(void (*function)(void* arg), void* arg) = 0;// 表示可以灵活调度各种任务, 这些任务由传入的函数参数和指针来定义

  // Start a new thread, invoking "function(arg)" within the new thread.
  // When "function(arg)" returns, the thread will be destroyed.
  virtual void StartThread(void (*function)(void* arg), void* arg) = 0;

  // *path is set to a temporary directory that can be used for testing. It may
  // or may not have just been created. The directory may or may not differ
  // between runs of the same process, but subsequent calls will return the
  // same directory.
  virtual Status GetTestDirectory(std::string* path) = 0;

  // Create and return a log file for storing informational messages.
  virtual Status NewLogger(const std::string& fname, Logger** result) = 0;

  // Returns the number of micro-seconds since some fixed point in time. Only
  // useful for computing deltas of time.
  virtual uint64_t NowMicros() = 0;

  // Sleep/delay the thread for the prescribed number of micro-seconds.
  virtual void SleepForMicroseconds(int micros) = 0;
};

// A file abstraction for reading sequentially through a file
class LEVELDB_EXPORT SequentialFile {
 public:
  SequentialFile() = default;

  SequentialFile(const SequentialFile&) = delete;
  SequentialFile& operator=(const SequentialFile&) = delete;

  virtual ~SequentialFile();

  // Read up to "n" bytes from the file.  "scratch[0..n-1]" may be
  // written by this routine.  Sets "*result" to the data that was
  // read (including if fewer than "n" bytes were successfully read).
  // May set "*result" to point at data in "scratch[0..n-1]", so
  // "scratch[0..n-1]" must be live when "*result" is used.
  // If an error was encountered, returns a non-OK status.
  //
  // REQUIRES: External synchronization
  virtual Status Read(size_t n, Slice* result, char* scratch) = 0;

  // Skip "n" bytes from the file. This is guaranteed to be no
  // slower that reading the same data, but may be faster.
  //
  // If end of file is reached, skipping will stop at the end of the
  // file, and Skip will return OK.
  //
  // REQUIRES: External synchronization
  virtual Status Skip(uint64_t n) = 0;
};

// A file abstraction for randomly reading the contents of a file.
class LEVELDB_EXPORT RandomAccessFile {
 public:
 // 表示 使用默认构造函数
  RandomAccessFile() = default;

  RandomAccessFile(const RandomAccessFile&) = delete;
  RandomAccessFile& operator=(const RandomAccessFile&) = delete;

  virtual ~RandomAccessFile();

// 创建 虚函数 Read
  virtual Status Read(uint64_t offset, size_t n, Slice* result,
                      char* scratch) const = 0;
};

// 
class LEVELDB_EXPORT WritableFile {
 public:
  WritableFile() = default;

  WritableFile(const WritableFile&) = delete;
  WritableFile& operator=(const WritableFile&) = delete;

  virtual ~WritableFile();

  virtual Status Append(const Slice& data) = 0;
  virtual Status Close() = 0;
  virtual Status Flush() = 0;
  virtual Status Sync() = 0;
};

// An interface for writing log messages.
class LEVELDB_EXPORT Logger {
 public:
  Logger() = default;

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  virtual ~Logger();

  // Write an entry to the log file with the specified format.
  virtual void Logv(const char* format, std::va_list ap) = 0;
};

// Identifies a locked file.
class LEVELDB_EXPORT FileLock {
 public:
  FileLock() = default;

  FileLock(const FileLock&) = delete;
  FileLock& operator=(const FileLock&) = delete;

  virtual ~FileLock();
};

// Log the specified data to *info_log if info_log is non-null.
void Log(Logger* info_log, const char* format, ...)
#if defined(__GNUC__) || defined(__clang__)
    __attribute__((__format__(__printf__, 2, 3)))
#endif
    ;

// A utility routine: write "data" to the named file.
LEVELDB_EXPORT Status WriteStringToFile(Env* env, const Slice& data,
                                        const std::string& fname);

// A utility routine: read contents of named file into *data
LEVELDB_EXPORT Status ReadFileToString(Env* env, const std::string& fname,
                                       std::string* data);

// An implementation of Env that forwards all calls to another Env.
// May be useful to clients who wish to override just part of the
// functionality of another Env.
class LEVELDB_EXPORT EnvWrapper : public Env {// 创建一个继承类
 public:
  // Initialize an EnvWrapper that delegates all calls to *t.
  explicit EnvWrapper(Env* t) : target_(t) {}// 防止隐式调用
  virtual ~EnvWrapper();// 可以根据基类的 析构函数 来调用对应的实现类的 析构函数

  // Return the target to which this Env forwards all calls.
  Env* target() const { return target_; }// 使用target 来返回

  // The following text is boilerplate that forwards all methods to target().
  Status NewSequentialFile(const std::string& f, SequentialFile** r) override {
    return target_->NewSequentialFile(f, r);
  }
  Status NewRandomAccessFile(const std::string& f,
                             RandomAccessFile** r) override {
    return target_->NewRandomAccessFile(f, r);
  }
  Status NewWritableFile(const std::string& f, WritableFile** r) override {
    return target_->NewWritableFile(f, r);
  }
  Status NewAppendableFile(const std::string& f, WritableFile** r) override {
    return target_->NewAppendableFile(f, r);
  }
  bool FileExists(const std::string& f) override {
    return target_->FileExists(f);
  }
  Status GetChildren(const std::string& dir,
                     std::vector<std::string>* r) override {
    return target_->GetChildren(dir, r);
  }
  Status RemoveFile(const std::string& f) override {
    return target_->RemoveFile(f);
  }
  Status CreateDir(const std::string& d) override {
    return target_->CreateDir(d);
  }
  Status RemoveDir(const std::string& d) override {
    return target_->RemoveDir(d);
  }
  Status GetFileSize(const std::string& f, uint64_t* s) override {
    return target_->GetFileSize(f, s);
  }
  Status RenameFile(const std::string& s, const std::string& t) override {
    return target_->RenameFile(s, t);
  }
  Status LockFile(const std::string& f, FileLock** l) override {
    return target_->LockFile(f, l);
  }
  Status UnlockFile(FileLock* l) override { return target_->UnlockFile(l); }
  void Schedule(void (*f)(void*), void* a) override {
    return target_->Schedule(f, a);// 
  }
  void StartThread(void (*f)(void*), void* a) override {
    return target_->StartThread(f, a);
  }
  Status GetTestDirectory(std::string* path) override {
    return target_->GetTestDirectory(path);
  }
  Status NewLogger(const std::string& fname, Logger** result) override {
    return target_->NewLogger(fname, result);
  }
  uint64_t NowMicros() override { return target_->NowMicros(); }
  void SleepForMicroseconds(int micros) override {
    target_->SleepForMicroseconds(micros);
  }

 private:
  Env* target_;// 这个是重点, 这个类相当于对原本的Env包括了一层, 目的是什么?
};

}  // namespace leveldb

// This workaround can be removed when leveldb::Env::DeleteFile is removed.
// Redefine DeleteFile if it was undefined earlier.
// 这里是为了解决函数定义的问题
#if defined(_WIN32) && defined(LEVELDB_DELETEFILE_UNDEFINED)
#if defined(UNICODE)// 是否定义 UNICODE
#define DeleteFile DeleteFileW // 定义 DeleteFile DeleteFileW
#else
#define DeleteFile DeleteFileA
#endif  // defined(UNICODE)
#endif  // defined(_WIN32) && defined(LEVELDB_DELETEFILE_UNDEFINED)

#endif  // STORAGE_LEVELDB_INCLUDE_ENV_H_
