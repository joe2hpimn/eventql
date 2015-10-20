/**
 * Copyright (c) 2015 - The CM Authors <legal@clickmatcher.com>
 *   All Rights Reserved.
 *
 * This file is CONFIDENTIAL -- Distribution or duplication of this material or
 * the information contained herein is strictly forbidden unless prior written
 * permission is obtained.
 */
#include "zbase/mapreduce/tasks/ReturnResultsTask.h"

using namespace stx;

namespace zbase {

ReturnResultsTask::ReturnResultsTask(
    Vector<RefPtr<MapReduceTask>> sources) :
    sources_(sources) {}


} // namespace zbase

