// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "db/builder.h"

#include "db/dbformat.h"
#include "db/filename.h"
#include "db/table_cache.h"
#include "db/version_edit.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "leveldb/iterator.h"

namespace leveldb {
/**
 * 根据表的注释可以看出 这个方法主要是为了构建一个表
 * 2024031: 
 * dbname:表示数据库名称
 * env: 环境的配置
 * option: 表示一些配置选项
 * table_cache: 配置缓存
 * iter: 一些迭代器
 * meta: 元数据信息 
 * 
 */
Status BuildTable(const std::string& dbname, Env* env, const Options& options,
                  TableCache* table_cache, Iterator* iter, FileMetaData* meta) {
  Status s;
  meta->file_size = 0;
  iter->SeekToFirst();

/**
 * 返回一个表名称对象
 */
  std::string fname = TableFileName(dbname, meta->number);
  
  if (iter->Valid()) {
    WritableFile* file;
    s = env->NewWritableFile(fname, &file);
    if (!s.ok()) {
      return s;
    }
    /**
     * 创建一个表构建的类
     * 1.
     */
    TableBuilder* builder = new TableBuilder(options, file);
    /**
     * 输入 iter->key, 对这个数值进行编码 存入 smallest中
     */
    meta->smallest.DecodeFrom(iter->key());
    Slice key;
    for (; iter->Valid(); iter->Next()) {
      key = iter->key();
      builder->Add(key, iter->value());
    }
    if (!key.empty()) {
      meta->largest.DecodeFrom(key);
    }

    s = builder->Finish();// 判断表是不是正常结束
    if (s.ok()) {
      meta->file_size = builder->FileSize();
      assert(meta->file_size > 0);
    }
    delete builder;

    // 完成文件操作并检查文件错误
    if (s.ok()) {
      s = file->Sync();
    }
    if (s.ok()) {
      s = file->Close();
    }
    // 删除文件对象
    delete file;
    file = nullptr;

    // 如果状态正常，验证表是否可用
    if (s.ok()) {
      Iterator* it = table_cache->NewIterator(ReadOptions(), meta->number,
                                              meta->file_size);
      s = it->status();
      delete it;
    }
  }

  // 检查输入迭代器是否有错误
  if (!iter->status().ok()) {
    s = iter->status();
  }

  // 如果状态正常且文件大小大于 0，保留文件
  if (s.ok() && meta->file_size > 0) {
    // Keep it
  } else {
    // 否则删除文件
    env->RemoveFile(fname);
  }
  // 返回最终状态
  return s;
}

}  // namespace leveldb
