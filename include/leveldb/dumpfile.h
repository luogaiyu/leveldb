// Copyright (c) 2014 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_
#define STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_

#include <string>

#include "leveldb/env.h"
#include "leveldb/export.h"
#include "leveldb/status.h"

// 这段代码定义了一个辅助工具函数 DumpFile，
// 用于将 LevelDB 存储文件的内容导出为可读的文本格式，通常用于调试或数据审查。以下是对代码重点和设计的详细解释。
// DumpFile 的主要作用是：

// 将指定的 LevelDB 存储文件（如 SST 文件）的内容转换为文本形式。
// 输出结果是按行分隔的文本，每行对应文件中的一个条目。
// 调用 WritableFile 的 Append() 方法逐行将文本写入目标输出（如文件或内存缓冲区）。
namespace leveldb {

// Dump the contents of the file named by fname in text format to
// *dst.  Makes a sequence of dst->Append() calls; each call is passed
// the newline-terminated text corresponding to a single item found
// in the file.
//
// Returns a non-OK result if fname does not name a leveldb storage
// file, or if the file cannot be read.
LEVELDB_EXPORT Status DumpFile(Env* env, const std::string& fname,
                               WritableFile* dst);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_INCLUDE_DUMPFILE_H_
