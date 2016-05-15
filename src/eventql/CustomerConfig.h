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
#pragma once
#include "eventql/util/stdtypes.h"
#include "eventql/util/autoref.h"
#include "eventql/util/protobuf/MessageObject.h"
#include "eventql/CustomerConfigProtos.pb.h"

using namespace stx;

namespace zbase {

struct CustomerConfigRef : public RefCounted {
  CustomerConfigRef(CustomerConfig _config) : config(_config) {}
  CustomerConfig config;
};

CustomerConfig createCustomerConfig(const String& customer);

void eventDefinitonRemoveField(EventDefinition* def, const String& field);

void eventDefinitonAddField(
    EventDefinition* def,
    const String& field,
    uint32_t id,
    msg::FieldType type,
    bool repeated,
    bool optional);

} // namespace zbase

