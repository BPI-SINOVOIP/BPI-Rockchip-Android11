/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server.connectivity.ipmemorystore;

import static android.net.ipmemorystore.Status.ERROR_DATABASE_CANNOT_BE_OPENED;
import static android.net.ipmemorystore.Status.ERROR_GENERIC;
import static android.net.ipmemorystore.Status.ERROR_ILLEGAL_ARGUMENT;
import static android.net.ipmemorystore.Status.SUCCESS;

import static com.android.server.connectivity.ipmemorystore.IpMemoryStoreDatabase.EXPIRY_ERROR;
import static com.android.server.connectivity.ipmemorystore.RegularMaintenanceJobService.InterruptMaintenance;

import android.content.Context;
import android.database.SQLException;
import android.database.sqlite.SQLiteDatabase;
import android.net.IIpMemoryStore;
import android.net.ipmemorystore.Blob;
import android.net.ipmemorystore.IOnBlobRetrievedListener;
import android.net.ipmemorystore.IOnL2KeyResponseListener;
import android.net.ipmemorystore.IOnNetworkAttributesRetrievedListener;
import android.net.ipmemorystore.IOnSameL3NetworkResponseListener;
import android.net.ipmemorystore.IOnStatusAndCountListener;
import android.net.ipmemorystore.IOnStatusListener;
import android.net.ipmemorystore.NetworkAttributes;
import android.net.ipmemorystore.NetworkAttributesParcelable;
import android.net.ipmemorystore.SameL3NetworkResponse;
import android.net.ipmemorystore.Status;
import android.net.ipmemorystore.StatusParcelable;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.internal.annotations.VisibleForTesting;

import java.io.File;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * Implementation for the IP memory store.
 * This component offers specialized services for network components to store and retrieve
 * knowledge about networks, and provides intelligence that groups level 2 networks together
 * into level 3 networks.
 *
 * @hide
 */
public class IpMemoryStoreService extends IIpMemoryStore.Stub {
    private static final String TAG = IpMemoryStoreService.class.getSimpleName();
    private static final int DATABASE_SIZE_THRESHOLD = 10 * 1024 * 1024; //10MB
    private static final int MAX_DROP_RECORD_TIMES = 500;
    private static final int MIN_DELETE_NUM = 5;
    private static final boolean DBG = true;

    // Error codes below are internal and used for notifying status beteween IpMemoryStore modules.
    static final int ERROR_INTERNAL_BASE = -1_000_000_000;
    // This error code is used for maintenance only to notify RegularMaintenanceJobService that
    // full maintenance job has been interrupted.
    static final int ERROR_INTERNAL_INTERRUPTED = ERROR_INTERNAL_BASE - 1;

    @NonNull
    final Context mContext;
    @Nullable
    final SQLiteDatabase mDb;
    @NonNull
    final ExecutorService mExecutor;

    /**
     * Construct an IpMemoryStoreService object.
     * This constructor will block on disk access to open the database.
     * @param context the context to access storage with.
     */
    public IpMemoryStoreService(@NonNull final Context context) {
        // Note that constructing the service will access the disk and block
        // for some time, but it should make no difference to the clients. Because
        // the interface is one-way, clients fire and forget requests, and the callback
        // will get called eventually in any case, and the framework will wait for the
        // service to be created to deliver subsequent requests.
        // Avoiding this would mean the mDb member can't be final, which means the service would
        // have to test for nullity, care for failure, and allow for a wait at every single access,
        // which would make the code a lot more complex and require all methods to possibly block.
        mContext = context;
        SQLiteDatabase db;
        final IpMemoryStoreDatabase.DbHelper helper = new IpMemoryStoreDatabase.DbHelper(context);
        try {
            db = helper.getWritableDatabase();
            if (null == db) Log.e(TAG, "Unexpected null return of getWriteableDatabase");
        } catch (final SQLException e) {
            Log.e(TAG, "Can't open the Ip Memory Store database", e);
            db = null;
        } catch (final Exception e) {
            Log.wtf(TAG, "Impossible exception Ip Memory Store database", e);
            db = null;
        }
        mDb = db;
        // The single thread executor guarantees that all work is executed sequentially on the
        // same thread, and no two tasks can be active at the same time. This is required to
        // ensure operations from multiple clients don't interfere with each other (in particular,
        // operations involving a transaction must not run concurrently with other operations
        // as the other operations might be taken as part of the transaction). By default, the
        // single thread executor runs off an unbounded queue.
        // TODO : investigate replacing this scheme with a scheme where each thread has its own
        // instance of the database, as it may be faster. It is likely however that IpMemoryStore
        // operations are mostly IO-bound anyway, and additional contention is unlikely to bring
        // benefits. Alternatively, a read-write lock might increase throughput. Also if doing
        // this work, care must be taken around the privacy-preserving VACUUM operations as
        // VACUUM will fail if there are other open transactions at the same time, and using
        // multiple threads will open the possibility of this failure happening, threatening
        // the privacy guarantees.
        mExecutor = Executors.newSingleThreadExecutor();
        RegularMaintenanceJobService.schedule(mContext, this);
    }

    /**
     * Shutdown the memory store service, cancelling running tasks and dropping queued tasks.
     *
     * This is provided to give a way to clean up, and is meant to be available in case of an
     * emergency shutdown.
     */
    public void shutdown() {
        // By contrast with ExecutorService#shutdown, ExecutorService#shutdownNow tries
        // to cancel the existing tasks, and does not wait for completion. It does not
        // guarantee the threads can be terminated in any given amount of time.
        mExecutor.shutdownNow();
        if (mDb != null) mDb.close();
        RegularMaintenanceJobService.unschedule(mContext);
    }

    /** Helper function to make a status object */
    private StatusParcelable makeStatus(final int code) {
        return new Status(code).toParcelable();
    }

    /**
     * Store network attributes for a given L2 key.
     *
     * @param l2Key The L2 key for the L2 network. Clients that don't know or care about the L2
     *              key and only care about grouping can pass a unique ID here like the ones
     *              generated by {@code java.util.UUID.randomUUID()}, but keep in mind the low
     *              relevance of such a network will lead to it being evicted soon if it's not
     *              refreshed. Use findL2Key to try and find a similar L2Key to these attributes.
     * @param attributes The attributes for this network.
     * @param listener A listener to inform of the completion of this call, or null if the client
     *        is not interested in learning about success/failure.
     * Through the listener, returns the L2 key. This is useful if the L2 key was not specified.
     * If the call failed, the L2 key will be null.
     */
    // Note that while l2Key and attributes are non-null in spirit, they are received from
    // another process. If the remote process decides to ignore everything and send null, this
    // process should still not crash.
    @Override
    public void storeNetworkAttributes(@Nullable final String l2Key,
            @Nullable final NetworkAttributesParcelable attributes,
            @Nullable final IOnStatusListener listener) {
        // Because the parcelable is 100% mutable, the thread may not see its members initialized.
        // Therefore either an immutable object is created on this same thread before it's passed
        // to the executor, or there need to be a write barrier here and a read barrier in the
        // remote thread.
        final NetworkAttributes na = null == attributes ? null : new NetworkAttributes(attributes);
        mExecutor.execute(() -> {
            try {
                final int code = storeNetworkAttributesAndBlobSync(l2Key, na,
                        null /* clientId */, null /* name */, null /* data */);
                if (null != listener) listener.onComplete(makeStatus(code));
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Store a binary blob associated with an L2 key and a name.
     *
     * @param l2Key The L2 key for this network.
     * @param clientId The ID of the client.
     * @param name The name of this data.
     * @param blob The data to store.
     * @param listener The listener that will be invoked to return the answer, or null if the
     *        is not interested in learning about success/failure.
     * Through the listener, returns a status to indicate success or failure.
     */
    @Override
    public void storeBlob(@Nullable final String l2Key, @Nullable final String clientId,
            @Nullable final String name, @Nullable final Blob blob,
            @Nullable final IOnStatusListener listener) {
        final byte[] data = null == blob ? null : blob.data;
        mExecutor.execute(() -> {
            try {
                final int code = storeNetworkAttributesAndBlobSync(l2Key,
                        null /* NetworkAttributes */, clientId, name, data);
                if (null != listener) listener.onComplete(makeStatus(code));
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Helper method for storeNetworkAttributes and storeBlob.
     *
     * Either attributes or none of clientId, name and data may be null. This will write the
     * passed data if non-null, and will write attributes if non-null, but in any case it will
     * bump the relevance up.
     * Returns a success code from Status.
     */
    private int storeNetworkAttributesAndBlobSync(@Nullable final String l2Key,
            @Nullable final NetworkAttributes attributes,
            @Nullable final String clientId,
            @Nullable final String name, @Nullable final byte[] data) {
        if (null == l2Key) return ERROR_ILLEGAL_ARGUMENT;
        if (null == attributes && null == data) return ERROR_ILLEGAL_ARGUMENT;
        if (null != data && (null == clientId || null == name)) return ERROR_ILLEGAL_ARGUMENT;
        if (null == mDb) return ERROR_DATABASE_CANNOT_BE_OPENED;
        try {
            final long oldExpiry = IpMemoryStoreDatabase.getExpiry(mDb, l2Key);
            final long newExpiry = RelevanceUtils.bumpExpiryDate(
                    oldExpiry == EXPIRY_ERROR ? System.currentTimeMillis() : oldExpiry);
            final int errorCode =
                    IpMemoryStoreDatabase.storeNetworkAttributes(mDb, l2Key, newExpiry, attributes);
            // If no blob to store, the client is interested in the result of storing the attributes
            if (null == data) return errorCode;
            // Otherwise it's interested in the result of storing the blob
            return IpMemoryStoreDatabase.storeBlob(mDb, l2Key, clientId, name, data);
        } catch (Exception e) {
            if (DBG) {
                Log.e(TAG, "Exception while storing for key {" + l2Key
                        + "} ; NetworkAttributes {" + (null == attributes ? "null" : attributes)
                        + "} ; clientId {" + (null == clientId ? "null" : clientId)
                        + "} ; name {" + (null == name ? "null" : name)
                        + "} ; data {" + Utils.byteArrayToString(data) + "}", e);
            }
        }
        return ERROR_GENERIC;
    }

    /**
     * Returns the best L2 key associated with the attributes.
     *
     * This will find a record that would be in the same group as the passed attributes. This is
     * useful to choose the key for storing a sample or private data when the L2 key is not known.
     * If multiple records are group-close to these attributes, the closest match is returned.
     * If multiple records have the same closeness, the one with the smaller (unicode codepoint
     * order) L2 key is returned.
     * If no record matches these attributes, null is returned.
     *
     * @param attributes The attributes of the network to find.
     * @param listener The listener that will be invoked to return the answer.
     * Through the listener, returns the L2 key if one matched, or null.
     */
    @Override
    public void findL2Key(@Nullable final NetworkAttributesParcelable attributes,
            @Nullable final IOnL2KeyResponseListener listener) {
        if (null == listener) return;
        mExecutor.execute(() -> {
            try {
                if (null == attributes) {
                    listener.onL2KeyResponse(makeStatus(ERROR_ILLEGAL_ARGUMENT), null);
                    return;
                }
                if (null == mDb) {
                    listener.onL2KeyResponse(makeStatus(ERROR_ILLEGAL_ARGUMENT), null);
                    return;
                }
                final String key = IpMemoryStoreDatabase.findClosestAttributes(mDb,
                        new NetworkAttributes(attributes));
                listener.onL2KeyResponse(makeStatus(SUCCESS), key);
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Returns whether, to the best of the store's ability to tell, the two specified L2 keys point
     * to the same L3 network. Group-closeness is used to determine this.
     *
     * @param l2Key1 The key for the first network.
     * @param l2Key2 The key for the second network.
     * @param listener The listener that will be invoked to return the answer.
     * Through the listener, a SameL3NetworkResponse containing the answer and confidence.
     */
    @Override
    public void isSameNetwork(@Nullable final String l2Key1, @Nullable final String l2Key2,
            @Nullable final IOnSameL3NetworkResponseListener listener) {
        if (null == listener) return;
        mExecutor.execute(() -> {
            try {
                if (null == l2Key1 || null == l2Key2) {
                    listener.onSameL3NetworkResponse(makeStatus(ERROR_ILLEGAL_ARGUMENT), null);
                    return;
                }
                if (null == mDb) {
                    listener.onSameL3NetworkResponse(makeStatus(ERROR_ILLEGAL_ARGUMENT), null);
                    return;
                }
                try {
                    final NetworkAttributes attr1 =
                            IpMemoryStoreDatabase.retrieveNetworkAttributes(mDb, l2Key1);
                    final NetworkAttributes attr2 =
                            IpMemoryStoreDatabase.retrieveNetworkAttributes(mDb, l2Key2);
                    if (null == attr1 || null == attr2) {
                        listener.onSameL3NetworkResponse(makeStatus(SUCCESS),
                                new SameL3NetworkResponse(l2Key1, l2Key2,
                                        -1f /* never connected */).toParcelable());
                        return;
                    }
                    final float confidence = attr1.getNetworkGroupSamenessConfidence(attr2);
                    listener.onSameL3NetworkResponse(makeStatus(SUCCESS),
                            new SameL3NetworkResponse(l2Key1, l2Key2, confidence).toParcelable());
                } catch (Exception e) {
                    listener.onSameL3NetworkResponse(makeStatus(ERROR_GENERIC), null);
                }
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Retrieve the network attributes for a key.
     * If no record is present for this key, this will return null attributes.
     *
     * @param l2Key The key of the network to query.
     * @param listener The listener that will be invoked to return the answer.
     * Through the listener, returns the network attributes and the L2 key associated with
     *         the query.
     */
    @Override
    public void retrieveNetworkAttributes(@Nullable final String l2Key,
            @Nullable final IOnNetworkAttributesRetrievedListener listener) {
        if (null == listener) return;
        mExecutor.execute(() -> {
            try {
                if (null == l2Key) {
                    listener.onNetworkAttributesRetrieved(
                            makeStatus(ERROR_ILLEGAL_ARGUMENT), l2Key, null);
                    return;
                }
                if (null == mDb) {
                    listener.onNetworkAttributesRetrieved(
                            makeStatus(ERROR_DATABASE_CANNOT_BE_OPENED), l2Key, null);
                    return;
                }
                try {
                    final NetworkAttributes attributes =
                            IpMemoryStoreDatabase.retrieveNetworkAttributes(mDb, l2Key);
                    listener.onNetworkAttributesRetrieved(makeStatus(SUCCESS), l2Key,
                            null == attributes ? null : attributes.toParcelable());
                } catch (final Exception e) {
                    listener.onNetworkAttributesRetrieved(makeStatus(ERROR_GENERIC), l2Key, null);
                }
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Retrieve previously stored private data.
     * If no data was stored for this L2 key and name this will return null.
     *
     * @param l2Key The L2 key.
     * @param clientId The id of the client that stored this data.
     * @param name The name of the data.
     * @param listener The listener that will be invoked to return the answer.
     * Through the listener, returns the private data if any or null if none, with the L2 key
     *         and the name of the data associated with the query.
     */
    @Override
    public void retrieveBlob(@NonNull final String l2Key, @NonNull final String clientId,
            @NonNull final String name, @NonNull final IOnBlobRetrievedListener listener) {
        if (null == listener) return;
        mExecutor.execute(() -> {
            try {
                if (null == l2Key) {
                    listener.onBlobRetrieved(makeStatus(ERROR_ILLEGAL_ARGUMENT), l2Key, name, null);
                    return;
                }
                if (null == mDb) {
                    listener.onBlobRetrieved(makeStatus(ERROR_DATABASE_CANNOT_BE_OPENED), l2Key,
                            name, null);
                    return;
                }
                try {
                    final Blob b = new Blob();
                    b.data = IpMemoryStoreDatabase.retrieveBlob(mDb, l2Key, clientId, name);
                    listener.onBlobRetrieved(makeStatus(SUCCESS), l2Key, name, b);
                } catch (final Exception e) {
                    listener.onBlobRetrieved(makeStatus(ERROR_GENERIC), l2Key, name, null);
                }
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Delete a single entry.
     *
     * @param l2Key The L2 key of the entry to delete.
     * @param needWipe Whether the data must be wiped from disk immediately for security reasons.
     *                 This is very expensive and makes no functional difference ; only pass
     *                 true if security requires this data must be removed from disk immediately.
     * @param listener A listener that will be invoked to inform of the completion of this call,
     *                 or null if the client is not interested in learning about success/failure.
     * returns (through the listener) A status to indicate success and the number of deleted records
     */
    public void delete(@NonNull final String l2Key, final boolean needWipe,
            @Nullable final IOnStatusAndCountListener listener) {
        mExecutor.execute(() -> {
            try {
                final StatusAndCount res = IpMemoryStoreDatabase.delete(mDb, l2Key, needWipe);
                if (null != listener) listener.onComplete(makeStatus(res.status), res.count);
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Delete all entries in a cluster.
     *
     * This method will delete all entries in the memory store that have the cluster attribute
     * passed as an argument.
     *
     * @param cluster The cluster to delete.
     * @param needWipe Whether the data must be wiped from disk immediately for security reasons.
     *                 This is very expensive and makes no functional difference ; only pass
     *                 true if security requires this data must be removed from disk immediately.
     * @param listener A listener that will be invoked to inform of the completion of this call,
     *                 or null if the client is not interested in learning about success/failure.
     * returns (through the listener) A status to indicate success and the number of deleted records
     */
    public void deleteCluster(@NonNull final String cluster, final boolean needWipe,
            @Nullable final IOnStatusAndCountListener listener) {
        mExecutor.execute(() -> {
            try {
                final StatusAndCount res =
                        IpMemoryStoreDatabase.deleteCluster(mDb, cluster, needWipe);
                if (null != listener) listener.onComplete(makeStatus(res.status), res.count);
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    /**
     * Wipe the data in IpMemoryStore database upon network factory reset.
     */
    @Override
    public void factoryReset() {
        mExecutor.execute(() -> IpMemoryStoreDatabase.wipeDataUponNetworkReset(mDb));
    }

    /** Get db size threshold. */
    @VisibleForTesting
    protected int getDbSizeThreshold() {
        return DATABASE_SIZE_THRESHOLD;
    }

    private long getDbSize() {
        final File dbFile = new File(mDb.getPath());
        try {
            return dbFile.length();
        } catch (final SecurityException e) {
            if (DBG) Log.e(TAG, "Read db size access deny.", e);
            // Return zero value if can't get disk usage exactly.
            return 0;
        }
    }

    /** Check if db size is over the threshold. */
    @VisibleForTesting
    boolean isDbSizeOverThreshold() {
        return getDbSize() > getDbSizeThreshold();
    }

    /**
     * Full maintenance.
     *
     * @param listener A listener to inform of the completion of this call.
     */
    void fullMaintenance(@NonNull final IOnStatusListener listener,
            @NonNull final InterruptMaintenance interrupt) {
        mExecutor.execute(() -> {
            try {
                if (null == mDb) {
                    listener.onComplete(makeStatus(ERROR_DATABASE_CANNOT_BE_OPENED));
                    return;
                }

                // Interrupt maintenance because the scheduling job has been canceled.
                if (checkForInterrupt(listener, interrupt)) return;

                int result = SUCCESS;
                // Drop all records whose relevance has decayed to zero.
                // This is the first step to decrease memory store size.
                result = IpMemoryStoreDatabase.dropAllExpiredRecords(mDb);

                if (checkForInterrupt(listener, interrupt)) return;

                // Aggregate historical data in passes
                // TODO : Waiting for historical data implement.

                // Check if db size meets the storage goal(10MB). If not, keep dropping records and
                // aggregate historical data until the storage goal is met. Use for loop with 500
                // times restriction to prevent infinite loop (Deleting records always fail and db
                // size is still over the threshold)
                for (int i = 0; isDbSizeOverThreshold() && i < MAX_DROP_RECORD_TIMES; i++) {
                    if (checkForInterrupt(listener, interrupt)) return;

                    final int totalNumber = IpMemoryStoreDatabase.getTotalRecordNumber(mDb);
                    final long dbSize = getDbSize();
                    final float decreaseRate = (dbSize == 0)
                            ? 0 : (float) (dbSize - getDbSizeThreshold()) / (float) dbSize;
                    final int deleteNumber = Math.max(
                            (int) (totalNumber * decreaseRate), MIN_DELETE_NUM);

                    result = IpMemoryStoreDatabase.dropNumberOfRecords(mDb, deleteNumber);

                    if (checkForInterrupt(listener, interrupt)) return;

                    // Aggregate historical data
                    // TODO : Waiting for historical data implement.
                }
                listener.onComplete(makeStatus(result));
            } catch (final RemoteException e) {
                // Client at the other end died
            }
        });
    }

    private boolean checkForInterrupt(@NonNull final IOnStatusListener listener,
            @NonNull final InterruptMaintenance interrupt) throws RemoteException {
        if (!interrupt.isInterrupted()) return false;
        listener.onComplete(makeStatus(ERROR_INTERNAL_INTERRUPTED));
        return true;
    }

    @Override
    public int getInterfaceVersion() {
        return this.VERSION;
    }

    @Override
    public String getInterfaceHash() {
        return this.HASH;
    }
}
