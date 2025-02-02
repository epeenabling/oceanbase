/*
*  Copyright (c) 2021 Ant Group CO., Ltd.
*  OceanBase is licensed under Mulan PubL v1.
*  You can use this software according to the terms and conditions of the Mulan PubL v1.
*  You may obtain a copy of Mulan PubL v1 at: http://license.coscl.org.cn/MulanPubL-1.0
*  THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
*  EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
*  MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*  See the Mulan PubL v1 for more details.
*/
#ifndef OCEANBASE_SHARE_OB_TABLET_MEDIUM_SNAPSHOT_TABLE_OPERATOR_
#define OCEANBASE_SHARE_OB_TABLET_MEDIUM_SNAPSHOT_TABLE_OPERATOR_

#include "lib/container/ob_iarray.h"
#include "lib/mysqlclient/ob_isql_client.h"
#include "common/ob_zone.h"
#include "lib/mysqlclient/ob_mysql_transaction.h"
#include "share/ob_ls_id.h"
#include "share/tablet/ob_tablet_info.h"
#include "observer/ob_server_struct.h"

namespace oceanbase
{
namespace common
{
class ObMySQLTransaction;
}
namespace share
{
class ObTabletReplicaFilter;
class SCN;

// part compaction related member from __all_tablet_meta_table
struct ObTabletCompactionScnInfo
{
public:
  ObTabletCompactionScnInfo()
   : tenant_id_(OB_INVALID_TENANT_ID),
     ls_id_(0),
     tablet_id_(0),
     compaction_scn_(0),
     report_scn_(0),
     status_(ObTabletReplica::SCN_STATUS_MAX)
   {}
  ObTabletCompactionScnInfo(
      const int64_t tenant_id,
      const ObLSID &ls_id,
      const ObTabletID &tablet_id,
      const ObTabletReplica::ScnStatus status)
   : tenant_id_(tenant_id),
     ls_id_(ls_id.id()),
     tablet_id_(tablet_id.id()),
     compaction_scn_(0),
     report_scn_(0),
     status_(status)
   {}
  bool is_valid() const
  {
    return is_valid_tenant_id(tenant_id_) && ls_id_ > 0 && tablet_id_ > 0 && report_scn_ >= 0;
  }
  // only check when last compaction type is major
  bool could_schedule_next_round(const int64_t major_frozen_scn)
  {
    return ObTabletReplica::SCN_STATUS_IDLE == status_ && major_frozen_scn <= report_scn_;
  }
  void reset()
  {
    tenant_id_ = OB_INVALID_TENANT_ID;
    ls_id_ = 0;
    tablet_id_ = 0;
    compaction_scn_ = 0;
    report_scn_ = 0;
    status_ = ObTabletReplica::SCN_STATUS_MAX;
  }
  TO_STRING_KV(K_(tenant_id), K_(ls_id), K_(tablet_id), K_(compaction_scn), K_(report_scn), K_(status));
public:
  uint64_t tenant_id_;
  int64_t ls_id_;
  int64_t tablet_id_;
  int64_t compaction_scn_;
  int64_t report_scn_;
  ObTabletReplica::ScnStatus status_;
};

// CRUD operation to __all_tablet_meta_table
class ObTabletMetaTableCompactionOperator
{
public:
  static int set_info_status(
      const ObTabletCompactionScnInfo &input_info,
      ObTabletCompactionScnInfo &ret_info);
  static int get_status(
      const ObTabletCompactionScnInfo &input_info,
      ObTabletCompactionScnInfo &ret_info);
  static int diagnose_compaction_scn(
      const int64_t tenant_id,
      int64_t &error_tablet_cnt);
  // update report_scn of all tablets which belong to @tablet_pairs
  static int batch_update_report_scn(
      const uint64_t tenant_id,
      const uint64_t global_braodcast_scn_val,
      const common::ObIArray<ObTabletLSPair> &tablet_pairs,
      const ObTabletReplica::ScnStatus &except_status);
  // after major_freeze, update all tablets' report_scn to global_braodcast_scn_val
  static int batch_update_report_scn(
      const uint64_t tenant_id,
      const uint64_t global_braodcast_scn_val,
      const ObTabletReplica::ScnStatus &except_status);
  // designed for 'clear merge error'. it updates all tablets' status to SCN_STATUS_IDLE
  static int batch_update_status(const uint64_t tenant_id, const int64_t expected_epoch);
  static int get_unique_status(
      const uint64_t tenant_id,
      common::ObIArray<ObTabletLSPair> &pairs,
      common::ObIArray<ObTabletReplica::ScnStatus> &status_arr);
  static int batch_update_unequal_report_scn_tablet(
      const uint64_t tenant_id,
      const share::ObLSID &ls_id,
      const int64_t major_frozen_scn,
      const common::ObIArray<ObTabletID> &input_tablet_id_array);
  static int get_min_compaction_scn(
      const uint64_t tenant_id,
      SCN &min_compaction_scn);
private:
  // is_update_finish_scn = TRUE: update finish_scn
  // is_update_finish_scn = FALSE: delete rows
  static int inner_batch_update_with_trans(
      common::ObMySQLTransaction &trans,
      const uint64_t tenant_id,
      const bool is_update_finish_scn,
      const common::ObIArray<share::ObTabletReplica> &replicas);
  static int do_select(
      ObISQLClient &sql_client,
      const bool select_with_update,
      const ObTabletCompactionScnInfo &input_info,
      ObTabletCompactionScnInfo &ret_info);
  static int execute_select_sql(
      ObISQLClient &sql_client,
      const int64_t meta_tenant_id,
      const ObSqlString &sql,
      ObTabletCompactionScnInfo &ret_info);
  static void handle_trans_stat(common::ObMySQLTransaction &trans, int &ret);
  // construct compaction_scn_info based on part of the fileds defined in the schema
  static int construct_compaction_related_info(
      sqlclient::ObMySQLResult &result,
      ObTabletCompactionScnInfo &info);
  static int inner_batch_update_unequal_report_scn_tablet(
      const uint64_t tenant_id,
      const share::ObLSID &ls_id,
      const int64_t major_frozen_scn,
      const common::ObIArray<ObTabletID> &unequal_tablet_id_array);
  static int append_tablet_id_array(
      const uint64_t tenant_id,
      const common::ObIArray<ObTabletID> &input_tablet_id_array,
      const int64_t start_idx,
      const int64_t end_idx,
      ObSqlString &sql);
  static int construct_unequal_tablet_id_array(
      sqlclient::ObMySQLResult &result,
      common::ObIArray<ObTabletID> &unequal_tablet_id_array);
  static int get_estimated_timeout_us(const uint64_t tenant_id, int64_t &estimated_timeout_us);
  static int get_tablet_replica_cnt(const uint64_t tenant_id, int64_t &tablet_replica_cnt);
private:
  const static int64_t MAX_BATCH_COUNT = 150;
};

} // end namespace share
} // end namespace oceanbase

#endif  // OCEANBASE_SHARE_OB_TABLET_MEDIUM_SNAPSHOT_TABLE_OPERATOR_
