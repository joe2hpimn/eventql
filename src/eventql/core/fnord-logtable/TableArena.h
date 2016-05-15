/**
 * Copyright (c) 2016 zScale Technology GmbH <legal@zscale.io>
 * Authors:
 *   - Paul Asmuth <paul@zscale.io>
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU Affero General Public License ("the license") as
 * published by the Free Software Foundation, either version 3 of the License,
 * or any later version.
 *
 * In accordance with Section 7(e) of the license, the licensing of the Program
 * under the license does not imply a trademark license. Therefore any rights,
 * title and interest in our trademarks remain entirely with us.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the license for more details.
 *
 * You can be released from the requirements of the license by purchasing a
 * commercial license. Buying such a license is mandatory as soon as you develop
 * commercial activities involving this program without disclosing the source
 * code of your own applications
 */
#ifndef _FNORD_LOGTABLE_TABLEARENA_H
#define _FNORD_LOGTABLE_TABLEARENA_H
#include <eventql/util/stdtypes.h>
#include <eventql/util/autoref.h>
#include <eventql/util/protobuf/MessageSchema.h>
#include <eventql/util/protobuf/MessageObject.h>

namespace stx {
namespace logtable {

class TableArena : public RefCounted {
public:

  TableArena(uint64_t start_sequence, const String& chunkid);

  void addRecord(const msg::MessageObject& record);

  const List<msg::MessageObject>& records() const;

  size_t startSequence() const;
  const String& chunkID() const;
  size_t size() const;

  bool isCommmited() const;
  void commit();

protected:
  size_t start_sequence_;
  String chunkid_;
  List<msg::MessageObject> records_;
  std::atomic<size_t> size_;
  std::atomic<size_t> is_committed_;
};

} // namespace logtable
} // namespace stx

#endif
