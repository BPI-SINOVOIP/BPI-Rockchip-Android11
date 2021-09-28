// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef IORAP_SRC_DB_MODELS_H_
#define IORAP_SRC_DB_MODELS_H_

#include "clean_up.h"
#include "file_models.h"

#include <android-base/logging.h>
#include <utils/String8.h>

#include <filesystem>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>
#include <sstream>
#include <type_traits>
#include <vector>

#include <sqlite3.h>

namespace iorap::db {

const constexpr int kDbVersion = 2;

struct SqliteDbDeleter {
  void operator()(sqlite3* db) {
    if (db != nullptr) {
      LOG(VERBOSE) << "sqlite3_close for: " << db;
      sqlite3_close(db);
    }
  }
};

class DbHandle {
 public:
  // Take over ownership of sqlite3 db.
  explicit DbHandle(sqlite3* db)
    : db_{std::shared_ptr<sqlite3>{db, SqliteDbDeleter{}}},
      mutex_{std::make_shared<std::mutex>()} {
  }

  sqlite3* get() {
    return db_.get();
  }

  operator sqlite3*() {
    return db_.get();
  }

  std::mutex& mutex() {
    return *mutex_.get();
  }

 private:
  std::shared_ptr<sqlite3> db_;
  std::shared_ptr<std::mutex> mutex_;
};

class ScopedLockDb {
 public:
  ScopedLockDb(std::mutex& mutex) : mutex(mutex) {
    mutex.lock();
  }

  ScopedLockDb(DbHandle& handle) : ScopedLockDb(handle.mutex()) {
  }

  ~ScopedLockDb() {
    mutex.unlock();
  }
 private:
  std::mutex& mutex;
};

class DbStatement {
 public:
  template <typename ... Args>
  static DbStatement Prepare(DbHandle db, const std::string& sql, Args&&... args) {
    return Prepare(db, sql.c_str(), std::forward<Args>(args)...);
  }

  template <typename ... Args>
  static DbStatement Prepare(DbHandle db, const char* sql, Args&&... args) {
    DCHECK(db.get() != nullptr);
    DCHECK(sql != nullptr);

    // LOG(VERBOSE) << "Prepare DB=" << db.get();

    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db.get(), sql, -1, /*out*/&stmt, nullptr);

    DbStatement db_stmt{db, stmt};
    DCHECK(db_stmt.CheckOk(rc)) << sql;
    db_stmt.BindAll(std::forward<Args>(args)...);

    return db_stmt;
  }

  DbStatement(DbHandle db, sqlite3_stmt* stmt) : db_(db), stmt_(stmt) {
  }

  sqlite3_stmt* get() {
    return stmt_;
  }

  DbHandle db() {
    return db_;
  }

  // Successive BindAll calls *do not* start back at the 0th bind position.
  template <typename T, typename ... Args>
  void BindAll(T&& arg, Args&&... args) {
    Bind(std::forward<T>(arg));
    BindAll(std::forward<Args>(args)...);
  }

  void BindAll() {}

  template <typename T>
  void Bind(const std::optional<T>& value) {
    if (value) {
      Bind(*value);
    } else {
      BindNull();
    }
  }

  void Bind(bool value) {
    CheckOk(sqlite3_bind_int(stmt_, bind_counter_++, value));
  }

  void Bind(int value) {
    CheckOk(sqlite3_bind_int(stmt_, bind_counter_++, value));
  }

  void Bind(uint64_t value) {
    CheckOk(sqlite3_bind_int64(stmt_, bind_counter_++, static_cast<int64_t>(value)));
  }

  void Bind(const char* value) {
    if (value != nullptr) {
      //sqlite3_bind_text(stmt_, /*index*/bind_counter_++, value, -1, SQLITE_STATIC);
      CheckOk(sqlite3_bind_text(stmt_, /*index*/bind_counter_++, value, -1, SQLITE_TRANSIENT));
    } else {
      BindNull();
    }
  }

  void Bind(const std::string& value) {
    Bind(value.c_str());
  }

  template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
  void Bind(E value) {
    Bind(static_cast<std::underlying_type_t<E>>(value));
  }

  void BindNull() {
    CheckOk(sqlite3_bind_null(stmt_, bind_counter_++));
  }

  int Step() {
    ++step_counter_;
    return sqlite3_step(stmt_);
  }

  bool Step(int expected) {
    int rc = Step();
    if (rc != expected) {
      LOG(ERROR) << "SQLite error: " << sqlite3_errmsg(db_.get());
      return false;
    }
    return true;
  }

  // Successive ColumnAll calls start at the 0th column again.
  template <typename T, typename ... Args>
  void ColumnAll(T& first, Args&... rest) {
    Column(first);
    ColumnAll(rest...);
    // Reset column counter back to 0
    column_counter_ = 0;
  }

  void ColumnAll() {}

  template <typename T>
  void Column(std::optional<T>& value) {
    T tmp;
    Column(/*out*/tmp);

    if (!tmp) {  // disambiguate 0 and NULL
      const unsigned char* text = sqlite3_column_text(stmt_, column_counter_ - 1);
      if (text == nullptr) {
        value = std::nullopt;
        return;
      }
    }
    value = std::move(tmp);
  }

  template <typename E, typename = std::enable_if_t<std::is_enum_v<E>>>
  void Column(E& value) {
    std::underlying_type_t<E> tmp;
    Column(/*out*/tmp);
    value = static_cast<E>(tmp);
  }

  void Column(bool& value) {
    value = sqlite3_column_int(stmt_, column_counter_++);
  }

  void Column(int& value) {
    value = sqlite3_column_int(stmt_, column_counter_++);
  }

  void Column(uint64_t& value) {
    value = static_cast<uint64_t>(sqlite3_column_int64(stmt_, column_counter_++));
  }

  void Column(std::string& value) {
    const unsigned char* text = sqlite3_column_text(stmt_, column_counter_++);

    DCHECK(text != nullptr) << "Column should be marked NOT NULL, otherwise use optional<string>";
    if (text == nullptr) {
      LOG(ERROR) << "Got NULL back for column " << column_counter_-1
                 << "; is this column marked NOT NULL?";
      value = "(((null)))";  // Don't segfault, keep going.
      return;
    }

    value = std::string{reinterpret_cast<const char*>(text)};
  }

  const char* ExpandedSql() {
    char* p = sqlite3_expanded_sql(stmt_);
    if (p == nullptr) {
      return "(nullptr)";
    }
    return p;
  }

  const char* Sql() {
    const char* p = sqlite3_sql(stmt_);
    if (p == nullptr) {
      return "(nullptr)";
    }
    return p;
  }


  DbStatement(DbStatement&& other)
    : db_{other.db_}, stmt_{other.stmt_}, bind_counter_{other.bind_counter_},
      step_counter_{other.step_counter_} {
    other.db_ = DbHandle{nullptr};
    other.stmt_ = nullptr;
  }

  ~DbStatement() {
    if (stmt_ != nullptr) {
      DCHECK_GT(step_counter_, 0) << " forgot to call Step()?";
      sqlite3_finalize(stmt_);
    }
  }

 private:
  bool CheckOk(int rc, int expected = SQLITE_OK) {
    if (rc != expected) {
      LOG(ERROR) << "Got error for SQL query: '" << Sql() << "'"
                 << ", expanded: '" << ExpandedSql() << "'";
      LOG(ERROR) << "Failed SQLite api call (" << rc << "): " << sqlite3_errstr(rc);
    }
    return rc == expected;
  }

  DbHandle db_;
  sqlite3_stmt* stmt_;
  int bind_counter_ = 1;
  int step_counter_ = 0;
  int column_counter_ = 0;
};

class DbQueryBuilder {
 public:
  // Returns the row ID that was inserted last.
  template <typename... Args>
  static std::optional<int> Insert(DbHandle db, const std::string& sql, Args&&... args) {
    ScopedLockDb lock{db};

    sqlite3_int64 last_rowid = sqlite3_last_insert_rowid(db.get());
    DbStatement stmt = DbStatement::Prepare(db, sql, std::forward<Args>(args)...);

    if (!stmt.Step(SQLITE_DONE)) {
      return std::nullopt;
    }

    last_rowid = sqlite3_last_insert_rowid(db.get());
    DCHECK_GT(last_rowid, 0);

    return static_cast<int>(last_rowid);
  }

  template <typename... Args>
  static bool Delete(DbHandle db, const std::string& sql, Args&&... args) {
    return ExecuteOnce(db, sql, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static bool Update(DbHandle db, const std::string& sql, Args&&... args) {
    return ExecuteOnce(db, sql, std::forward<Args>(args)...);
  }

  template <typename... Args>
  static bool ExecuteOnce(DbHandle db, const std::string& sql, Args&&... args) {
    ScopedLockDb lock{db};

    DbStatement stmt = DbStatement::Prepare(db, sql, std::forward<Args>(args)...);

    if (!stmt.Step(SQLITE_DONE)) {
      return false;
    }

    return true;
  }

  template <typename... Args>
  static bool SelectOnce(DbStatement& stmt, Args&... args) {
    int rc = stmt.Step();
    switch (rc) {
      case SQLITE_ROW:
        stmt.ColumnAll(/*out*/args...);
        return true;
      case SQLITE_DONE:
        return false;
      default:
        LOG(ERROR) << "Failed to step (" << rc << "): " << sqlite3_errmsg(stmt.db());
        return false;
    }
  }
};

class Model {
 public:
  DbHandle db() const {
    return db_;
  }

  Model(DbHandle db) : db_{db} {
  }

 private:
  DbHandle db_;
};

class SchemaModel : public Model {
 public:
  static SchemaModel GetOrCreate(std::string location) {
    int rc = sqlite3_config(SQLITE_CONFIG_LOG, ErrorLogCallback, /*data*/nullptr);

    if (rc != SQLITE_OK) {
      LOG(FATAL) << "Failed to configure logging";
    }

    sqlite3* db = nullptr;
    bool is_deprecated = false;
    if (location != ":memory:") {
      // Try to open DB if it already exists.
      rc = sqlite3_open_v2(location.c_str(), /*out*/&db, SQLITE_OPEN_READWRITE, /*vfs*/nullptr);

      if (rc == SQLITE_OK) {
        LOG(INFO) << "Opened existing database at '" << location << "'";
        SchemaModel schema{DbHandle{db}, location};
        if (schema.Version() == kDbVersion) {
          return schema;
        } else {
          LOG(DEBUG) << "The version is old, reinit the db."
                     << " old version is "
                     << schema.Version()
                     << " and new version is "
                     << kDbVersion;
          CleanUpFilesForDb(schema.db());
          is_deprecated = true;
       }
      }
    }

    if (is_deprecated) {
      // Remove the db and recreate it.
      // TODO: migrate to a newer version without deleting the old one.
      std::filesystem::remove(location.c_str());
    }

    // Create a new DB if one didn't exist already.
    rc = sqlite3_open(location.c_str(), /*out*/&db);

    if (rc != SQLITE_OK) {
      LOG(FATAL) << "Failed to open DB: " << sqlite3_errmsg(db);
    }

    SchemaModel schema{DbHandle{db}, location};
    schema.Reinitialize();
    // TODO: migrate versions upwards when we rev the schema version

    int old_version = schema.Version();
    LOG(VERBOSE) << "Loaded schema version: " << old_version;

    return schema;
  }

  void MarkSingleton() {
    s_singleton_ = db();
  }

  static DbHandle GetSingleton() {
    DCHECK(s_singleton_.has_value());
    return *s_singleton_;
  }

  void Reinitialize() {
    const char* sql_to_initialize = R"SQLC0D3(
        DROP TABLE IF EXISTS schema_versions;
        DROP TABLE IF EXISTS packages;
        DROP TABLE IF EXISTS activities;
        DROP TABLE IF EXISTS app_launch_histories;
        DROP TABLE IF EXISTS raw_traces;
        DROP TABLE IF EXISTS prefetch_files;
)SQLC0D3";
    char* err_msg = nullptr;
    int rc = sqlite3_exec(db().get(),
                          sql_to_initialize,
                          /*callback*/nullptr,
                          /*arg*/0,
                          /*out*/&err_msg);
    if (rc != SQLITE_OK) {
      LOG(FATAL) << "Failed to drop tables: " << err_msg ? err_msg : "nullptr";
    }

    CreateSchema();
    LOG(INFO) << "Reinitialized database at '" << location_ << "'";
  }

  int Version() {
    std::string query = "SELECT MAX(version) FROM schema_versions;";
    DbStatement stmt = DbStatement::Prepare(db(), query);

    int return_value = 0;
    if (!DbQueryBuilder::SelectOnce(stmt, /*out*/return_value)) {
      LOG(ERROR) << "Failed to query schema version";
      return -1;
    }

    return return_value;
  }

 protected:
  SchemaModel(DbHandle db, std::string location) : Model{db}, location_(location) {
  }

 private:
  static std::optional<DbHandle> s_singleton_;

  void CreateSchema() {
    const char* sql_to_initialize = R"SQLC0D3(
        CREATE TABLE schema_versions(
            version INTEGER NOT NULL
        );

        CREATE TABLE packages(
            id INTEGER NOT NULL,
            name TEXT NOT NULL,
            version INTEGER NOT NULL,

            PRIMARY KEY(id)
        );

        CREATE TABLE activities(
            id INTEGER NOT NULL,
            name TEXT NOT NULL,
            package_id INTEGER NOT NULL,

            PRIMARY KEY(id),
            FOREIGN KEY (package_id) REFERENCES packages (id) ON DELETE CASCADE
        );

        CREATE TABLE app_launch_histories(
            id INTEGER NOT NULL PRIMARY KEY,
            activity_id INTEGER NOT NULL,
            -- 1:Cold, 2:Warm, 3:Hot
            temperature INTEGER CHECK (temperature IN (1, 2, 3)) NOT NULL,
            trace_enabled INTEGER CHECK(trace_enabled in (TRUE, FALSE)) NOT NULL,
            readahead_enabled INTEGER CHECK(trace_enabled in (TRUE, FALSE)) NOT NULL,
            -- absolute timestamp since epoch
            intent_started_ns INTEGER CHECK(intent_started_ns IS NULL or intent_started_ns >= 0),
            -- absolute timestamp since epoch
            total_time_ns INTEGER CHECK(total_time_ns IS NULL or total_time_ns >= 0),
            -- absolute timestamp since epoch
            report_fully_drawn_ns INTEGER CHECK(report_fully_drawn_ns IS NULL or report_fully_drawn_ns >= 0),

            FOREIGN KEY (activity_id) REFERENCES activities (id) ON DELETE CASCADE
        );

        CREATE TABLE raw_traces(
            id INTEGER NOT NULL PRIMARY KEY,
            history_id INTEGER NOT NULL,
            file_path TEXT NOT NULL,

            FOREIGN KEY (history_id) REFERENCES app_launch_histories (id) ON DELETE CASCADE
        );

        CREATE TABLE prefetch_files(
          id INTEGER NOT NULL PRIMARY KEY,
          activity_id INTEGER NOT NULL,
          file_path TEXT NOT NULL,

          FOREIGN KEY (activity_id) REFERENCES activities (id) ON DELETE CASCADE
        );
)SQLC0D3";

    char* err_msg = nullptr;
    int rc = sqlite3_exec(db().get(),
                          sql_to_initialize,
                          /*callback*/nullptr,
                          /*arg*/0,
                          /*out*/&err_msg);

    if (rc != SQLITE_OK) {
      LOG(FATAL) << "Failed to create tables: " << err_msg ? err_msg : "nullptr";
    }

    const char* sql_to_insert_schema_version = R"SQLC0D3(
      INSERT INTO schema_versions VALUES(%d)
      )SQLC0D3";
    rc = sqlite3_exec(db().get(),
                      android::String8::format(sql_to_insert_schema_version,
                                               kDbVersion),
                      /*callback*/nullptr,
                      /*arg*/0,
                      /*out*/&err_msg);

    if (rc != SQLITE_OK) {
      LOG(FATAL) << "Failed to insert the schema version: "
                 << err_msg ? err_msg : "nullptr";
    }
  }

  static void ErrorLogCallback(void *pArg, int iErrCode, const char *zMsg) {
    LOG(ERROR) << "SQLite error (" << iErrCode << "): " << zMsg;
  }

  std::string location_;
};

class PackageModel : public Model {
 protected:
  PackageModel(DbHandle db) : Model{db} {
  }

 public:
  static std::optional<PackageModel> SelectById(DbHandle db, int id) {
    ScopedLockDb lock{db};
    int original_id = id;

    std::string query = "SELECT * FROM packages WHERE id = ?1 LIMIT 1;";
    DbStatement stmt = DbStatement::Prepare(db, query, id);

    PackageModel p{db};
    if (!DbQueryBuilder::SelectOnce(stmt, p.id, p.name, p.version)) {
      return std::nullopt;
    }

    return p;
  }

  static std::vector<PackageModel> SelectByName(DbHandle db, const char* name) {
    ScopedLockDb lock{db};

    std::string query = "SELECT * FROM packages WHERE name = ?1;";
    DbStatement stmt = DbStatement::Prepare(db, query, name);

    std::vector<PackageModel> packages;

    PackageModel p{db};
    while (DbQueryBuilder::SelectOnce(stmt, p.id, p.name, p.version)) {
      packages.push_back(p);
    }

    return packages;
  }

  static std::optional<PackageModel> SelectByNameAndVersion(DbHandle db,
                                                            const char* name,
                                                            int version) {
    ScopedLockDb lock{db};

    std::string query =
        "SELECT * FROM packages WHERE name = ?1 AND version = ?2 LIMIT 1;";
    DbStatement stmt = DbStatement::Prepare(db, query, name, version);

    PackageModel p{db};
    if (!DbQueryBuilder::SelectOnce(stmt, p.id, p.name, p.version)) {
      return std::nullopt;
    }

    return p;
  }

  static std::vector<PackageModel> SelectAll(DbHandle db) {
    ScopedLockDb lock{db};

    std::string query = "SELECT * FROM packages;";
    DbStatement stmt = DbStatement::Prepare(db, query);

    std::vector<PackageModel> packages;
    PackageModel p{db};
    while (DbQueryBuilder::SelectOnce(stmt, p.id, p.name, p.version)) {
      packages.push_back(p);
    }

    return packages;
  }

  static std::optional<PackageModel> Insert(DbHandle db,
                                            std::string name,
                                            int version) {
    const char* sql = "INSERT INTO packages (name, version) VALUES (?1, ?2);";

    std::optional<int> inserted_row_id =
        DbQueryBuilder::Insert(db, sql, name, version);
    if (!inserted_row_id) {
      return std::nullopt;
    }

    PackageModel p{db};
    p.name = name;
    p.version = version;
    p.id = *inserted_row_id;

    return p;
  }

  bool Delete() {
    const char* sql = "DELETE FROM packages WHERE id = ?";

    return DbQueryBuilder::Delete(db(), sql, id);
  }

  int id;
  std::string name;
  int version;
};

inline std::ostream& operator<<(std::ostream& os, const PackageModel& p) {
  os << "PackageModel{id=" << p.id << ",name=" << p.name << ",";
  os << "version=";
  os << p.version;
  os << "}";
  return os;
}

class ActivityModel : public Model {
 protected:
  ActivityModel(DbHandle db) : Model{db} {
  }

 public:
  static std::optional<ActivityModel> SelectById(DbHandle db, int id) {
    ScopedLockDb lock{db};
    int original_id = id;

    std::string query = "SELECT * FROM activities WHERE id = ? LIMIT 1;";
    DbStatement stmt = DbStatement::Prepare(db, query, id);

    ActivityModel p{db};
    if (!DbQueryBuilder::SelectOnce(stmt, p.id, p.name, p.package_id)) {
      return std::nullopt;
    }

    return p;
  }

  static std::optional<ActivityModel> SelectByNameAndPackageId(DbHandle db,
                                                               const char* name,
                                                               int package_id) {
    ScopedLockDb lock{db};

    std::string query = "SELECT * FROM activities WHERE name = ? AND package_id = ? LIMIT 1;";
    DbStatement stmt = DbStatement::Prepare(db, query, name, package_id);

    ActivityModel p{db};
    if (!DbQueryBuilder::SelectOnce(stmt, p.id, p.name, p.package_id)) {
      return std::nullopt;
    }

    return p;
  }

  static std::vector<ActivityModel> SelectByPackageId(DbHandle db,
                                                      int package_id) {
    ScopedLockDb lock{db};

    std::string query = "SELECT * FROM activities WHERE package_id = ?;";
    DbStatement stmt = DbStatement::Prepare(db, query, package_id);

    std::vector<ActivityModel> activities;
    ActivityModel p{db};
    while (DbQueryBuilder::SelectOnce(stmt, p.id, p.name, p.package_id)) {
      activities.push_back(p);
    }

    return activities;
  }

  static std::optional<ActivityModel> Insert(DbHandle db,
                                             std::string name,
                                             int package_id) {
    const char* sql = "INSERT INTO activities (name, package_id) VALUES (?1, ?2);";

    std::optional<int> inserted_row_id =
        DbQueryBuilder::Insert(db, sql, name, package_id);
    if (!inserted_row_id) {
      return std::nullopt;
    }

    ActivityModel p{db};
    p.id = *inserted_row_id;
    p.name = name;
    p.package_id = package_id;

    return p;
  }

  // Try to select by package_name+activity_name, otherwise insert into both tables.
  // Package version is ignored for selects.
  static std::optional<ActivityModel> SelectOrInsert(
      DbHandle db,
      std::string package_name,
      int package_version,
      std::string activity_name) {
    std::optional<PackageModel> package = PackageModel::SelectByNameAndVersion(db,
                                                                               package_name.c_str(),
                                                                               package_version);
    if (!package) {
      package = PackageModel::Insert(db, package_name, package_version);
      DCHECK(package.has_value());
    }

    std::optional<ActivityModel> activity =
        ActivityModel::SelectByNameAndPackageId(db,
                                                activity_name.c_str(),
                                                package->id);
    if (!activity) {
      activity = Insert(db, activity_name, package->id);
      // XX: should we really return an optional here? This feels like it should never fail.
    }

    return activity;
  }

  int id;
  std::string name;
  int package_id;  // PackageModel::id
};

inline std::ostream& operator<<(std::ostream& os, const ActivityModel& p) {
  os << "ActivityModel{id=" << p.id << ",name=" << p.name << ",";
  os << "package_id=" << p.package_id << "}";
  return os;
}

class AppLaunchHistoryModel : public Model {
 protected:
  AppLaunchHistoryModel(DbHandle db) : Model{db} {
  }

 public:
  enum class Temperature : int32_t {
    kUninitialized = -1,  // Note: Not a valid SQL value.
    kCold = 1,
    kWarm = 2,
    kHot = 3,
  };

  static std::optional<AppLaunchHistoryModel> SelectById(DbHandle db, int id) {
    ScopedLockDb lock{db};
    int original_id = id;

    std::string query = "SELECT * FROM app_launch_histories WHERE id = ? LIMIT 1;";
    DbStatement stmt = DbStatement::Prepare(db, query, id);

    AppLaunchHistoryModel p{db};
    if (!DbQueryBuilder::SelectOnce(stmt,
                                    p.id,
                                    p.activity_id,
                                    p.temperature,
                                    p.trace_enabled,
                                    p.readahead_enabled,
                                    p.intent_started_ns,
                                    p.total_time_ns,
                                    p.report_fully_drawn_ns)) {
      return std::nullopt;
    }

    return p;
  }

  // Selects the activity history for an activity id.
  // The requirements are:
  // * Should be cold run.
  // * Pefetto trace is enabled.
  // * intent_start_ns is *NOT* null.
  static std::vector<AppLaunchHistoryModel> SelectActivityHistoryForCompile(
      DbHandle db,
      int activity_id) {
    ScopedLockDb lock{db};
    std::string query = "SELECT * FROM app_launch_histories "
                        "WHERE activity_id = ?1 AND"
                        "  temperature = 1 AND"
                        "  trace_enabled = TRUE AND"
                        "  intent_started_ns IS NOT NULL;";
    DbStatement stmt = DbStatement::Prepare(db, query, activity_id);
    std::vector<AppLaunchHistoryModel> result;

    AppLaunchHistoryModel p{db};
    while (DbQueryBuilder::SelectOnce(stmt,
                                      p.id,
                                      p.activity_id,
                                      p.temperature,
                                      p.trace_enabled,
                                      p.readahead_enabled,
                                      p.intent_started_ns,
                                      p.total_time_ns,
                                      p.report_fully_drawn_ns)) {
      result.push_back(p);
    }
    return result;
  }

  static std::optional<AppLaunchHistoryModel> Insert(DbHandle db,
                                                     int activity_id,
                                                     AppLaunchHistoryModel::Temperature temperature,
                                                     bool trace_enabled,
                                                     bool readahead_enabled,
                                                     std::optional<uint64_t> intent_started_ns,
                                                     std::optional<uint64_t> total_time_ns,
                                                     std::optional<uint64_t> report_fully_drawn_ns)
  {
    const char* sql = "INSERT INTO app_launch_histories (activity_id, temperature, trace_enabled, "
                                                        "readahead_enabled, intent_started_ns, "
                                                        "total_time_ns, report_fully_drawn_ns) "
                      "VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7);";

    std::optional<int> inserted_row_id =
        DbQueryBuilder::Insert(db,
                               sql,
                               activity_id,
                               temperature,
                               trace_enabled,
                               readahead_enabled,
                               intent_started_ns,
                               total_time_ns,
                               report_fully_drawn_ns);
    if (!inserted_row_id) {
      return std::nullopt;
    }

    AppLaunchHistoryModel p{db};
    p.id = *inserted_row_id;
    p.activity_id = activity_id;
    p.temperature = temperature;
    p.trace_enabled = trace_enabled;
    p.readahead_enabled = readahead_enabled;
    p.intent_started_ns = intent_started_ns;
    p.total_time_ns = total_time_ns;
    p.report_fully_drawn_ns = report_fully_drawn_ns;

    return p;
  }

  static bool UpdateReportFullyDrawn(DbHandle db,
                                     int history_id,
                                     uint64_t report_fully_drawn_ns)
  {
    const char* sql = "UPDATE app_launch_histories "
                      "SET report_fully_drawn_ns = ?1 "
                      "WHERE id = ?2;";

    bool result = DbQueryBuilder::Update(db, sql, report_fully_drawn_ns, history_id);

    if (!result) {
      LOG(ERROR)<< "Failed to update history_id:"<< history_id
                << ", report_fully_drawn_ns: " << report_fully_drawn_ns;
    }
    return result;
  }

  int id;
  int activity_id;  // ActivityModel::id
  Temperature temperature = Temperature::kUninitialized;
  bool trace_enabled;
  bool readahead_enabled;
  std::optional<uint64_t> intent_started_ns;
  std::optional<uint64_t> total_time_ns;
  std::optional<uint64_t> report_fully_drawn_ns;
};

inline std::ostream& operator<<(std::ostream& os, const AppLaunchHistoryModel& p) {
  os << "AppLaunchHistoryModel{id=" << p.id << ","
     << "activity_id=" << p.activity_id << ","
     << "temperature=" << static_cast<int>(p.temperature) << ","
     << "trace_enabled=" << p.trace_enabled << ","
     << "readahead_enabled=" << p.readahead_enabled << ","
     << "intent_started_ns=";
  if (p.intent_started_ns) {
    os << *p.intent_started_ns;
  } else {
    os << "(nullopt)";
  }
  os << ",";
  os << "total_time_ns=";
  if (p.total_time_ns) {
    os << *p.total_time_ns;
  } else {
    os << "(nullopt)";
  }
  os << ",";
  os << "report_fully_drawn_ns=";
  if (p.report_fully_drawn_ns) {
    os << *p.report_fully_drawn_ns;
  } else {
    os << "(nullopt)";
  }
  os << "}";
  return os;
}

class RawTraceModel : public Model {
 protected:
  RawTraceModel(DbHandle db) : Model{db} {
  }

 public:

  // Return raw_traces, sorted ascending by the id.
  static std::vector<RawTraceModel> SelectByVersionedComponentName(DbHandle db,
                                                                   VersionedComponentName vcn) {
    ScopedLockDb lock{db};

    const char* sql =
      "SELECT raw_traces.id, raw_traces.history_id, raw_traces.file_path "
      "FROM raw_traces "
      "INNER JOIN app_launch_histories ON raw_traces.history_id = app_launch_histories.id "
      "INNER JOIN activities ON activities.id = app_launch_histories.activity_id "
      "INNER JOIN packages ON packages.id = activities.package_id "
      "WHERE packages.name = ? AND activities.name = ? AND packages.version = ?"
      "ORDER BY raw_traces.id ASC";

    DbStatement stmt = DbStatement::Prepare(db,
                                            sql,
                                            vcn.GetPackage(),
                                            vcn.GetActivity(),
                                            vcn.GetVersion());

    std::vector<RawTraceModel> results;

    RawTraceModel p{db};
    while (DbQueryBuilder::SelectOnce(stmt, p.id, p.history_id, p.file_path)) {
      results.push_back(p);
    }

    return results;
  }

  static std::optional<RawTraceModel> SelectByHistoryId(DbHandle db, int history_id) {
    ScopedLockDb lock{db};

    const char* sql =
      "SELECT id, history_id, file_path "
      "FROM raw_traces "
      "WHERE history_id = ?1 "
      "LIMIT 1;";

    DbStatement stmt = DbStatement::Prepare(db, sql, history_id);

    RawTraceModel p{db};
    if (!DbQueryBuilder::SelectOnce(stmt, p.id, p.history_id, p.file_path)) {
      return std::nullopt;
    }

    return p;
  }

  static std::optional<RawTraceModel> Insert(DbHandle db,
                                             int history_id,
                                             std::string file_path) {
    const char* sql = "INSERT INTO raw_traces (history_id, file_path) VALUES (?1, ?2);";

    std::optional<int> inserted_row_id =
        DbQueryBuilder::Insert(db, sql, history_id, file_path);
    if (!inserted_row_id) {
      return std::nullopt;
    }

    RawTraceModel p{db};
    p.id = *inserted_row_id;
    p.history_id = history_id;
    p.file_path = file_path;

    return p;
  }

  bool Delete() {
    const char* sql = "DELETE FROM raw_traces WHERE id = ?";

    return DbQueryBuilder::Delete(db(), sql, id);
  }

  int id;
  int history_id;
  std::string file_path;
};

inline std::ostream& operator<<(std::ostream& os, const RawTraceModel& p) {
  os << "RawTraceModel{id=" << p.id << ",history_id=" << p.history_id << ",";
  os << "file_path=" << p.file_path << "}";
  return os;
}

class PrefetchFileModel : public Model {
 protected:
  PrefetchFileModel(DbHandle db) : Model{db} {
  }

 public:
  static std::optional<PrefetchFileModel> SelectByVersionedComponentName(
      DbHandle db,
      VersionedComponentName vcn) {
    ScopedLockDb lock{db};

    const char* sql =
      "SELECT prefetch_files.id, prefetch_files.activity_id, prefetch_files.file_path "
      "FROM prefetch_files "
      "INNER JOIN activities ON activities.id = prefetch_files.activity_id "
      "INNER JOIN packages ON packages.id = activities.package_id "
      "WHERE packages.name = ? AND activities.name = ? AND packages.version = ?";

    DbStatement stmt = DbStatement::Prepare(db,
                                            sql,
                                            vcn.GetPackage(),
                                            vcn.GetActivity(),
                                            vcn.GetVersion());

    PrefetchFileModel p{db};

    if (!DbQueryBuilder::SelectOnce(stmt, p.id, p.activity_id, p.file_path)) {
      return std::nullopt;
    }

    return p;
  }

  static std::vector<PrefetchFileModel> SelectAll(DbHandle db) {
    ScopedLockDb lock{db};

    std::string query =
      "SELECT prefetch_files.id, prefetch_files.activity_id, prefetch_files.file_path "
      "FROM prefetch_files";
    DbStatement stmt = DbStatement::Prepare(db, query);

    std::vector<PrefetchFileModel> prefetch_files;
    PrefetchFileModel p{db};
    while (DbQueryBuilder::SelectOnce(stmt, p.id, p.activity_id, p.file_path)) {
      prefetch_files.push_back(p);
    }

    return prefetch_files;
  }

  static std::optional<PrefetchFileModel> Insert(DbHandle db,
                                                 int activity_id,
                                                 std::string file_path) {
    const char* sql = "INSERT INTO prefetch_files (activity_id, file_path) VALUES (?1, ?2);";

    std::optional<int> inserted_row_id =
        DbQueryBuilder::Insert(db, sql, activity_id, file_path);
    if (!inserted_row_id) {
      return std::nullopt;
    }

    PrefetchFileModel p{db};
    p.id = *inserted_row_id;
    p.activity_id = activity_id;
    p.file_path = file_path;

    return p;
  }

  bool Delete() {
    const char* sql = "DELETE FROM prefetch_files WHERE id = ?";

    return DbQueryBuilder::Delete(db(), sql, id);
  }

  int id;
  int activity_id;
  std::string file_path;
};

inline std::ostream& operator<<(std::ostream& os, const PrefetchFileModel& p) {
  os << "PrefetchFileModel{id=" << p.id << ",activity_id=" << p.activity_id << ",";
  os << "file_path=" << p.file_path << "}";
  return os;
}

}  // namespace iorap::db

#endif  // IORAP_SRC_DB_MODELS_H_
