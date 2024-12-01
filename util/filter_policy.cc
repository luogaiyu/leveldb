// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#include "leveldb/filter_policy.h"

namespace leveldb {

FilterPolicy::~FilterPolicy() {}// 虚析构函数确保在删除派生类对象时，基类的析构函数会被正确调用，从而保证资源的正确释放

}  // namespace leveldb
