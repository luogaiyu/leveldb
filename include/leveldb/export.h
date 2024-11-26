// Copyright (c) 2017 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
/**
 * 这段代码定义了 LevelDB 的动态链接库导出机制，核心功能是通过条件编译为动态库的符号导出定义正确的宏（LEVELDB_EXPORT）。以下是重点信息：

1. LEVELDB_EXPORT 宏简介
用途：用于标记需要在动态库中导出的符号（如类和函数）。
跨平台支持：同时支持 Windows 和非 Windows 平台的动态库导出。
静态库兼容：在非动态库编译时，LEVELDB_EXPORT 定义为空。
 */
#ifndef STORAGE_LEVELDB_INCLUDE_EXPORT_H_
#define STORAGE_LEVELDB_INCLUDE_EXPORT_H_

#if !defined(LEVELDB_EXPORT)

#if defined(LEVELDB_SHARED_LIBRARY)
#if defined(_WIN32)

#if defined(LEVELDB_COMPILE_LIBRARY)
#define LEVELDB_EXPORT __declspec(dllexport)
#else
#define LEVELDB_EXPORT __declspec(dllimport)
#endif  // defined(LEVELDB_COMPILE_LIBRARY)

#else  // defined(_WIN32)
#if defined(LEVELDB_COMPILE_LIBRARY)
#define LEVELDB_EXPORT __attribute__((visibility("default")))
#else
#define LEVELDB_EXPORT
#endif
#endif  // defined(_WIN32)

#else  // defined(LEVELDB_SHARED_LIBRARY)
#define LEVELDB_EXPORT
#endif

#endif  // !defined(LEVELDB_EXPORT)

#endif  // STORAGE_LEVELDB_INCLUDE_EXPORT_H_
