/**
 * Copyright 2016 Google Inc. All Rights Reserved.
 *
 * <p>Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 * <p>http://www.apache.org/licenses/LICENSE-2.0
 *
 * <p>Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.android.vts.util;


import com.android.vts.entity.ApiCoverageEntity;
import com.android.vts.entity.BranchEntity;
import com.android.vts.entity.BuildTargetEntity;
import com.android.vts.entity.CodeCoverageEntity;
import com.android.vts.entity.CoverageEntity;
import com.android.vts.entity.DeviceInfoEntity;
import com.android.vts.entity.ProfilingPointRunEntity;
import com.android.vts.entity.TestCaseRunEntity;
import com.android.vts.entity.TestEntity;
import com.android.vts.entity.TestPlanEntity;
import com.android.vts.entity.TestPlanRunEntity;
import com.android.vts.entity.TestRunEntity;
import com.android.vts.entity.TestRunEntity.TestRunType;
import com.android.vts.job.VtsAlertJobServlet;
import com.android.vts.job.VtsCoverageAlertJobServlet;
import com.android.vts.job.VtsProfilingStatsJobServlet;
import com.android.vts.proto.VtsReportMessage.AndroidDeviceInfoMessage;
import com.android.vts.proto.VtsReportMessage.ApiCoverageReportMessage;
import com.android.vts.proto.VtsReportMessage.CoverageReportMessage;
import com.android.vts.proto.VtsReportMessage.HalInterfaceMessage;
import com.android.vts.proto.VtsReportMessage.LogMessage;
import com.android.vts.proto.VtsReportMessage.ProfilingReportMessage;
import com.android.vts.proto.VtsReportMessage.TestCaseReportMessage;
import com.android.vts.proto.VtsReportMessage.TestCaseResult;
import com.android.vts.proto.VtsReportMessage.TestPlanReportMessage;
import com.android.vts.proto.VtsReportMessage.TestReportMessage;
import com.android.vts.proto.VtsReportMessage.UrlResourceMessage;
import com.google.appengine.api.datastore.DatastoreFailureException;
import com.google.appengine.api.datastore.DatastoreService;
import com.google.appengine.api.datastore.DatastoreServiceFactory;
import com.google.appengine.api.datastore.DatastoreTimeoutException;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.EntityNotFoundException;
import com.google.appengine.api.datastore.FetchOptions;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.appengine.api.datastore.Query;
import com.google.appengine.api.datastore.Query.Filter;
import com.google.appengine.api.datastore.Query.FilterOperator;
import com.google.appengine.api.datastore.Query.FilterPredicate;
import com.google.appengine.api.datastore.Transaction;
import com.google.appengine.api.datastore.TransactionOptions;
import com.google.common.collect.Lists;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.ConcurrentModificationException;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.stream.Collectors;

/**
 * DatastoreHelper, a helper class for interacting with Cloud Datastore.
 */
public class DatastoreHelper {

  /**
   * The default kind name for datastore
   */
  public static final String NULL_ENTITY_KIND = "nullEntity";

  public static final int MAX_WRITE_RETRIES = 5;
  /**
   * This variable is for maximum number of entities per transaction You can find the detail here
   * (https://cloud.google.com/datastore/docs/concepts/limits)
   */
  public static final int MAX_ENTITY_SIZE_PER_TRANSACTION = 300;

  protected static final Logger logger = Logger.getLogger(DatastoreHelper.class.getName());
  private static final DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();

  /**
   * Get query fetch options for large batches of entities.
   *
   * @return FetchOptions with a large chunk and prefetch size.
   */
  public static FetchOptions getLargeBatchOptions() {
    return FetchOptions.Builder.withChunkSize(1000).prefetchSize(1000);
  }

  /**
   * Returns true if there are data points newer than lowerBound in the results table.
   *
   * @param parentKey The parent key to use in the query.
   * @param kind The query entity kind.
   * @param lowerBound The (exclusive) lower time bound, long, microseconds.
   * @return boolean True if there are newer data points.
   */
  public static boolean hasNewer(Key parentKey, String kind, Long lowerBound) {
    if (lowerBound == null || lowerBound <= 0) {
      return false;
    }
    DatastoreService datastore = DatastoreServiceFactory.getDatastoreService();
    Key startKey = KeyFactory.createKey(parentKey, kind, lowerBound);
    Filter startFilter =
            new FilterPredicate(
                    Entity.KEY_RESERVED_PROPERTY, FilterOperator.GREATER_THAN, startKey);
    Query q = new Query(kind).setAncestor(parentKey).setFilter(startFilter).setKeysOnly();
    return datastore.prepare(q).countEntities(FetchOptions.Builder.withLimit(1)) > 0;
  }

  /**
   * Returns true if there are data points older than upperBound in the table.
   *
   * @param parentKey The parent key to use in the query.
   * @param kind The query entity kind.
   * @param upperBound The (exclusive) upper time bound, long, microseconds.
   * @return boolean True if there are older data points.
   */
  public static boolean hasOlder(Key parentKey, String kind, Long upperBound) {
    if (upperBound == null || upperBound <= 0) {
      return false;
    }
    Key endKey = KeyFactory.createKey(parentKey, kind, upperBound);
    Filter endFilter =
            new FilterPredicate(Entity.KEY_RESERVED_PROPERTY, FilterOperator.LESS_THAN, endKey);
    Query q = new Query(kind).setAncestor(parentKey).setFilter(endFilter).setKeysOnly();
    return datastore.prepare(q).countEntities(FetchOptions.Builder.withLimit(1)) > 0;
  }

  /**
   * Get all of the devices branches.
   *
   * @return a list of all branches.
   */
  public static List<String> getAllBranches() {
    Query query = new Query(BranchEntity.KIND).setKeysOnly();
    List<String> branches = new ArrayList<>();
    for (Entity e : datastore.prepare(query).asIterable(getLargeBatchOptions())) {
      branches.add(e.getKey().getName());
    }
    return branches;
  }

  /**
   * Get all of the device build flavors.
   *
   * @return a list of all device build flavors.
   */
  public static List<String> getAllBuildFlavors() {
    Query query = new Query(BuildTargetEntity.KIND).setKeysOnly();
    List<String> devices = new ArrayList<>();
    for (Entity e : datastore.prepare(query).asIterable(getLargeBatchOptions())) {
      devices.add(e.getKey().getName());
    }
    return devices;
  }

  /**
   * Datastore Transactional process for data insertion with MAX_WRITE_RETRIES times and withXG of
   * false value
   *
   * @param entity The entity that you want to insert to datastore.
   * @param entityList The list of entity for using datastore put method.
   */
  private static boolean datastoreTransactionalRetry(Entity entity, List<Entity> entityList) {
    return datastoreTransactionalRetryWithXG(entity, entityList, false);
  }

  /**
   * Datastore Transactional process for data insertion with MAX_WRITE_RETRIES times
   *
   * @param entity The entity that you want to insert to datastore.
   * @param entityList The list of entity for using datastore put method.
   */
  private static boolean datastoreTransactionalRetryWithXG(
          Entity entity, List<Entity> entityList, boolean withXG) {
    int retries = 0;
    while (true) {
      Transaction txn;
      if (withXG) {
        TransactionOptions options = TransactionOptions.Builder.withXG(withXG);
        txn = datastore.beginTransaction(options);
      } else {
        txn = datastore.beginTransaction();
      }

      try {
        // Check if test already exists in the database
        if (!entity.getKind().equalsIgnoreCase(NULL_ENTITY_KIND)) {
          try {
            if (entity.getKind().equalsIgnoreCase("Test")) {
              Entity datastoreEntity = datastore.get(entity.getKey());
              TestEntity datastoreTestEntity = TestEntity.fromEntity(datastoreEntity);
              if (datastoreTestEntity == null
                      || !datastoreTestEntity.equals(entity)) {
                entityList.add(entity);
              }
            } else if (entity.getKind().equalsIgnoreCase("TestPlan")) {
              datastore.get(entity.getKey());
            } else {
              datastore.get(entity.getKey());
            }
          } catch (EntityNotFoundException e) {
            entityList.add(entity);
          }
        }
        datastore.put(txn, entityList);
        txn.commit();
        break;
      } catch (ConcurrentModificationException
              | DatastoreFailureException
              | DatastoreTimeoutException e) {
        entityList.remove(entity);
        logger.log(
                Level.WARNING,
                "Retrying insert kind: " + entity.getKind() + " key: " + entity.getKey());
        if (retries++ >= MAX_WRITE_RETRIES) {
          logger.log(
                  Level.SEVERE,
                  "Exceeded maximum retries kind: "
                          + entity.getKind()
                          + " key: "
                          + entity.getKey());
          return false;
        }
      } finally {
        if (txn.isActive()) {
          logger.log(
                  Level.WARNING, "Transaction rollback forced for : " + entity.getKind());
          txn.rollback();
        }
      }
    }
    return true;
  }
}
