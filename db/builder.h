// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

// 这是一个头文件，定义了构建表文件的函数声明。
// 该文件包含了一些必要的头文件和命名空间声明。
// 主要功能是根据迭代器的内容构建一个表文件，并根据meta->number命名生成的文件。
// 成功时，meta的其余部分将填充生成的表的元数据。
// 如果迭代器中没有数据，meta->file_size将被设置为零，并且不会生成表文件。

#ifndef STORAGE_LEVELDB_DB_BUILDER_H_
#define STORAGE_LEVELDB_DB_BUILDER_H_

#include "leveldb/status.h"

namespace leveldb {
// struct默认是 private, class 默认是 public
struct Options; // 
struct FileMetaData;

class Env;
class Iterator;
class TableCache;
class VersionEdit;

// 从*iter的内容构建一个表文件。生成的文件将根据meta->number命名。
// 成功时，meta的其余部分将填充生成的表的元数据。
// 如果*iter中没有数据，meta->file_size将被设置为零，并且不会生成表文件。
// 通过 Status 来判断 这个建表的操作
Status BuildTable(const std::string& dbname, Env* env, const Options& options,
                  TableCache* table_cache, Iterator* iter, FileMetaData* meta);

}  // namespace leveldb

#endif  // STORAGE_LEVELDB_DB_BUILDER_H_
