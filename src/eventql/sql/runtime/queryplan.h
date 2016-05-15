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
#include <eventql/util/stdtypes.h>
#include <eventql/util/autoref.h>
#include <eventql/sql/qtree/QueryTreeNode.h>
#include <eventql/sql/scheduler.h>
#include <eventql/sql/transaction.h>

namespace csql {
class Runtime;
class ResultList;

class QueryPlan : public RefCounted {
public:

  /**
   * This constructor isn't usually directly called by users but invoked through
   * Runtime::buildQueryPlan
   */
  QueryPlan(
      Transaction* txn,
      Vector<RefPtr<QueryTreeNode>> qtrees);

  /**
   * Execute one of the statements in the query plan. The statement is referenced
   * by index. The index must be in the range  [0, numStatements)
   *
   * This method returns a result cursor. The underlying query will be executed
   * incrementally as result rows are read from the cursor
   */
  ScopedPtr<ResultCursor> execute(size_t stmt_idx);

  /**
   * Execute one of the statements in the query plan. The statement is referenced
   * by index. The index must be in the range  [0, numStatements)
   *
   * This method materializes the full result list into the provided result list
   * object
   */
  void execute(size_t stmt_idx, ResultList* result_list);

  /**
   * Retruns the number of statements in the query plan
   */
  size_t numStatements() const;

  /**
   * Returns the result column list ("header") for a statement in the query plan.
   * The statement is referenced by index. The index must be in the range
   * [0, numStatements)
   */
  const Vector<String>& getStatementOutputColumns(size_t stmt_idx);

  void setScheduler(RefPtr<Scheduler> scheduler);
  RefPtr<QueryTreeNode> getStatement(size_t stmt_idx) const;

  Transaction* getTransaction() const;

  //void onOutputComplete(size_t stmt_idx, Function<void ()> fn);
  //void onOutputRow(size_t stmt_idx, RowSinkFn fn);
  //void onQueryFinished(Function<void ()> fn);

  //void storeResults(size_t stmt_idx, ResultList* result_list);

protected:
  Transaction* txn_;
  Vector<RefPtr<QueryTreeNode>> qtrees_;
  Vector<Vector<String>> statement_columns_;
  RefPtr<Scheduler> scheduler_;
};

}
