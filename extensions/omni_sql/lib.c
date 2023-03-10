/**
 * @file lib.c
 *
 */

// clang-format off
#include <postgres.h>
#include <fmgr.h>
// clang-format on

#include <funcapi.h>
#include <parser/parser.h>
#include <utils/builtins.h>

#include <nodes/nodeFuncs.h>

#include "deparse.h"

List *omni_sql_parse_statement(char *statement) {
#if PG_MAJORVERSION_NUM == 13
  List *stmts = raw_parser(statement);
#else
  List *stmts = raw_parser(statement, RAW_PARSE_DEFAULT);
#endif
  return stmts;
}

bool omni_sql_get_with_clause(Node *node, WithClause ***with) {
  Node *curNode = node;
dispatch:
  switch (nodeTag(curNode)) {
  case T_RawStmt: {
    RawStmt *stmt = castNode(RawStmt, curNode);
    curNode = stmt->stmt;
    goto dispatch;
  }
  case T_SelectStmt: {
    SelectStmt *select = castNode(SelectStmt, curNode);
    *with = &select->withClause;
    break;
  }
  case T_InsertStmt: {
    InsertStmt *insert = castNode(InsertStmt, curNode);
    *with = &insert->withClause;
    break;
  }
  case T_UpdateStmt: {
    UpdateStmt *update = castNode(UpdateStmt, curNode);
    *with = &update->withClause;
    break;
  }
  case T_DeleteStmt: {
    DeleteStmt *delete = castNode(DeleteStmt, curNode);
    *with = &delete->withClause;
    break;
  }
#if PG_VERSION_NUM >= 15
  case T_MergeStmt: {
    MergeStmt *delete = castNode(MergeStmt, curNode);
    *with = &delete->withClause;
    break;
  }
#endif
  default:
    return false;
  }
  return true;
}

List *omni_sql_add_cte(List *stmts, text *cte_name, List *cte_stmts, bool recursive, bool prepend) {

  if (list_length(stmts) != 1) {
    ereport(ERROR, errmsg("Statement should contain one and only one statement"));
  }

  if (list_length(cte_stmts) != 1) {
    ereport(ERROR, errmsg("CTE should contain one and only one statement"));
  }

  void *node = linitial(stmts);

  CommonTableExpr *cte_node = makeNode(CommonTableExpr);
  cte_node->ctename = text_to_cstring(cte_name);
  cte_node->ctematerialized = CTEMaterializeDefault;
  cte_node->ctequery = linitial_node(RawStmt, cte_stmts)->stmt;
  cte_node->cterecursive = recursive;

  WithClause **with;
  if (!omni_sql_get_with_clause(node, &with)) {
    ereport(ERROR, errmsg("no supported statement found"));
  }

  if (*with == NULL) {
    WithClause *new_with = makeNode(WithClause);
    new_with->location = -1;
    new_with->recursive = recursive;
    new_with->ctes = list_make1(cte_node);
    *with = new_with;
  } else {
    List *ctes = (*with)->ctes;
    if (prepend) {
      ctes = list_insert_nth(ctes, 0, cte_node);
    } else {
      ctes = lappend(ctes, cte_node);
    }
  }

  return stmts;
}

static bool contains_param_walker(Node *node, bool *contains) {
  if (node != NULL) {
    if (nodeTag(node) == T_ParamRef) {
      *contains = true;
      return true;
    } else {
      return raw_expression_tree_walker(node, contains_param_walker, contains);
    }
  }
  return false;
}

bool omni_sql_is_parameterized(List *stmts) {

  ListCell *stmt;
  foreach (stmt, stmts) {
    bool contains = false;
    raw_expression_tree_walker(castNode(RawStmt, lfirst(stmt))->stmt, contains_param_walker,
                               &contains);
    if (contains) {
      return true;
    }
  }
  return false;
}