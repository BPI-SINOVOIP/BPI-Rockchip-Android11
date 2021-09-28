/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.google.android.exoplayer2.upstream.cache;

import android.net.Uri;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import com.google.android.exoplayer2.C;
import com.google.android.exoplayer2.upstream.DataSink;
import com.google.android.exoplayer2.upstream.DataSource;
import com.google.android.exoplayer2.upstream.DataSourceException;
import com.google.android.exoplayer2.upstream.DataSpec;
import com.google.android.exoplayer2.upstream.DummyDataSource;
import com.google.android.exoplayer2.upstream.FileDataSource;
import com.google.android.exoplayer2.upstream.PriorityDataSource;
import com.google.android.exoplayer2.upstream.TeeDataSource;
import com.google.android.exoplayer2.upstream.TransferListener;
import com.google.android.exoplayer2.upstream.cache.Cache.CacheException;
import com.google.android.exoplayer2.util.Assertions;
import com.google.android.exoplayer2.util.PriorityTaskManager;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.lang.annotation.Documented;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Collections;
import java.util.List;
import java.util.Map;
import org.checkerframework.checker.nullness.qual.MonotonicNonNull;

/**
 * A {@link DataSource} that reads and writes a {@link Cache}. Requests are fulfilled from the cache
 * when possible. When data is not cached it is requested from an upstream {@link DataSource} and
 * written into the cache.
 */
public final class CacheDataSource implements DataSource {

  /** {@link DataSource.Factory} for {@link CacheDataSource} instances. */
  public static final class Factory implements DataSource.Factory {

    private @MonotonicNonNull Cache cache;
    private DataSource.Factory cacheReadDataSourceFactory;
    @Nullable private DataSink.Factory cacheWriteDataSinkFactory;
    private CacheKeyFactory cacheKeyFactory;
    private boolean cacheIsReadOnly;
    @Nullable private DataSource.Factory upstreamDataSourceFactory;
    @Nullable private PriorityTaskManager upstreamPriorityTaskManager;
    private int upstreamPriority;
    @CacheDataSource.Flags private int flags;
    @Nullable private CacheDataSource.EventListener eventListener;

    public Factory() {
      cacheReadDataSourceFactory = new FileDataSource.Factory();
      cacheKeyFactory = CacheUtil.DEFAULT_CACHE_KEY_FACTORY;
    }

    /**
     * Sets the cache that will be used.
     *
     * <p>Must be called before the factory is used.
     *
     * @param cache The cache that will be used.
     * @return This factory.
     */
    public Factory setCache(Cache cache) {
      this.cache = cache;
      return this;
    }

    /**
     * Sets the {@link DataSource.Factory} for {@link DataSource DataSources} for reading from the
     * cache.
     *
     * <p>The default is a {@link FileDataSource.Factory} in its default configuration.
     *
     * @param cacheReadDataSourceFactory The {@link DataSource.Factory} for reading from the cache.
     * @return This factory.
     */
    public Factory setCacheReadDataSourceFactory(DataSource.Factory cacheReadDataSourceFactory) {
      this.cacheReadDataSourceFactory = cacheReadDataSourceFactory;
      return this;
    }

    /**
     * Sets the {@link DataSink.Factory} for generating {@link DataSink DataSinks} for writing data
     * to the cache. Passing {@code null} causes the cache to be read-only.
     *
     * <p>The default is a {@link CacheDataSink.Factory} in its default configuration.
     *
     * @param cacheWriteDataSinkFactory The {@link DataSink.Factory} for generating {@link DataSink
     *     DataSinks} for writing data to the cache, or {@code null} to disable writing.
     * @return This factory.
     */
    public Factory setCacheWriteDataSinkFactory(
        @Nullable DataSink.Factory cacheWriteDataSinkFactory) {
      this.cacheWriteDataSinkFactory = cacheWriteDataSinkFactory;
      this.cacheIsReadOnly = cacheWriteDataSinkFactory == null;
      return this;
    }

    /**
     * Sets the {@link CacheKeyFactory}.
     *
     * <p>The default is {@link CacheUtil#DEFAULT_CACHE_KEY_FACTORY}.
     *
     * @param cacheKeyFactory The {@link CacheKeyFactory}.
     * @return This factory.
     */
    public Factory setCacheKeyFactory(CacheKeyFactory cacheKeyFactory) {
      this.cacheKeyFactory = cacheKeyFactory;
      return this;
    }

    /**
     * Sets the {@link DataSource.Factory} for upstream {@link DataSource DataSources}, which are
     * used to read data in the case of a cache miss.
     *
     * <p>The default is {@code null}, and so this method must be called before the factory is used
     * in order for data to be read from upstream in the case of a cache miss.
     *
     * @param upstreamDataSourceFactory The upstream {@link DataSource} for reading data not in the
     *     cache, or {@code null} to cause failure in the case of a cache miss.
     * @return This factory.
     */
    public Factory setUpstreamDataSourceFactory(
        @Nullable DataSource.Factory upstreamDataSourceFactory) {
      this.upstreamDataSourceFactory = upstreamDataSourceFactory;
      return this;
    }

    /**
     * Sets an optional {@link PriorityTaskManager} to use when requesting data from upstream.
     *
     * <p>If set, reads from the upstream {@link DataSource} will only be allowed to proceed if
     * there are no higher priority tasks registered to the {@link PriorityTaskManager}. If there
     * exists a higher priority task then {@link PriorityTaskManager.PriorityTooLowException} will
     * be thrown instead.
     *
     * <p>Note that requests to {@link CacheDataSource} instances are intended to be used as parts
     * of (possibly larger) tasks that are registered with the {@link PriorityTaskManager}, and
     * hence {@link CacheDataSource} does <em>not</em> register a task by itself. This must be done
     * by the surrounding code that uses the {@link CacheDataSource} instances.
     *
     * <p>The default is {@code null}.
     *
     * @param upstreamPriorityTaskManager The upstream {@link PriorityTaskManager}.
     * @return This factory.
     */
    public Factory setUpstreamPriorityTaskManager(
        @Nullable PriorityTaskManager upstreamPriorityTaskManager) {
      this.upstreamPriorityTaskManager = upstreamPriorityTaskManager;
      return this;
    }

    /**
     * Sets the priority to use when requesting data from upstream. The priority is only used if a
     * {@link PriorityTaskManager} is set by calling {@link #setUpstreamPriorityTaskManager}.
     *
     * <p>The default is {@link C#PRIORITY_PLAYBACK}.
     *
     * @param upstreamPriority The priority to use when requesting data from upstream.
     * @return This factory.
     */
    public Factory setUpstreamPriority(int upstreamPriority) {
      this.upstreamPriority = upstreamPriority;
      return this;
    }

    /**
     * Sets the {@link CacheDataSource.Flags}.
     *
     * <p>The default is {@code 0}.
     *
     * @param flags The {@link CacheDataSource.Flags}.
     * @return This factory.
     */
    public Factory setFlags(@CacheDataSource.Flags int flags) {
      this.flags = flags;
      return this;
    }

    /**
     * Sets the {link EventListener} to which events are delivered.
     *
     * <p>The default is {@code null}.
     *
     * @param eventListener The {@link EventListener}.
     * @return This factory.
     */
    public Factory setEventListener(@Nullable EventListener eventListener) {
      this.eventListener = eventListener;
      return this;
    }

    @Override
    public CacheDataSource createDataSource() {
      return createDataSourceInternal(
          upstreamDataSourceFactory != null ? upstreamDataSourceFactory.createDataSource() : null,
          flags,
          upstreamPriority);
    }

    /**
     * Returns an instance suitable for downloading content. The created instance is equivalent to
     * one that would be created by {@link #createDataSource()}, except:
     *
     * <ul>
     *   <li>The {@link #FLAG_BLOCK_ON_CACHE} is always set.
     *   <li>The task priority is overridden to be {@link C#PRIORITY_DOWNLOAD}.
     * </ul>
     *
     * @return An instance suitable for downloading content.
     */
    public CacheDataSource createDataSourceForDownloading() {
      return createDataSourceInternal(
          upstreamDataSourceFactory != null ? upstreamDataSourceFactory.createDataSource() : null,
          flags | FLAG_BLOCK_ON_CACHE,
          C.PRIORITY_DOWNLOAD);
    }

    /**
     * Returns an instance suitable for reading cached content as part of removing a download. The
     * created instance is equivalent to one that would be created by {@link #createDataSource()},
     * except:
     *
     * <ul>
     *   <li>The upstream is overridden to be {@code null}, since when removing content we don't
     *       want to request anything that's not already cached.
     *   <li>The {@link #FLAG_BLOCK_ON_CACHE} is always set.
     *   <li>The task priority is overridden to be {@link C#PRIORITY_DOWNLOAD}.
     * </ul>
     *
     * @return An instance suitable for reading cached content as part of removing a download.
     */
    public CacheDataSource createDataSourceForRemovingDownload() {
      return createDataSourceInternal(
          /* upstreamDataSource= */ null, flags | FLAG_BLOCK_ON_CACHE, C.PRIORITY_DOWNLOAD);
    }

    private CacheDataSource createDataSourceInternal(
        @Nullable DataSource upstreamDataSource, @Flags int flags, int upstreamPriority) {
      Cache cache = Assertions.checkNotNull(this.cache);
      @Nullable DataSink cacheWriteDataSink;
      if (cacheIsReadOnly || upstreamDataSource == null) {
        cacheWriteDataSink = null;
      } else if (cacheWriteDataSinkFactory != null) {
        cacheWriteDataSink = cacheWriteDataSinkFactory.createDataSink();
      } else {
        cacheWriteDataSink = new CacheDataSink.Factory().setCache(cache).createDataSink();
      }
      return new CacheDataSource(
          cache,
          upstreamDataSource,
          cacheReadDataSourceFactory.createDataSource(),
          cacheWriteDataSink,
          cacheKeyFactory,
          flags,
          upstreamPriorityTaskManager,
          upstreamPriority,
          eventListener);
    }
  }

  /** Listener of {@link CacheDataSource} events. */
  public interface EventListener {

    /**
     * Called when bytes have been read from the cache.
     *
     * @param cacheSizeBytes Current cache size in bytes.
     * @param cachedBytesRead Total bytes read from the cache since this method was last called.
     */
    void onCachedBytesRead(long cacheSizeBytes, long cachedBytesRead);

    /**
     * Called when the current request ignores cache.
     *
     * @param reason Reason cache is bypassed.
     */
    void onCacheIgnored(@CacheIgnoredReason int reason);
  }

  /**
   * Flags controlling the CacheDataSource's behavior. Possible flag values are {@link
   * #FLAG_BLOCK_ON_CACHE}, {@link #FLAG_IGNORE_CACHE_ON_ERROR} and {@link
   * #FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS}.
   */
  @Documented
  @Retention(RetentionPolicy.SOURCE)
  @IntDef(
      flag = true,
      value = {
        FLAG_BLOCK_ON_CACHE,
        FLAG_IGNORE_CACHE_ON_ERROR,
        FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS
      })
  public @interface Flags {}
  /**
   * A flag indicating whether we will block reads if the cache key is locked. If unset then data is
   * read from upstream if the cache key is locked, regardless of whether the data is cached.
   */
  public static final int FLAG_BLOCK_ON_CACHE = 1;

  /**
   * A flag indicating whether the cache is bypassed following any cache related error. If set
   * then cache related exceptions may be thrown for one cycle of open, read and close calls.
   * Subsequent cycles of these calls will then bypass the cache.
   */
  public static final int FLAG_IGNORE_CACHE_ON_ERROR = 1 << 1; // 2

  /**
   * A flag indicating that the cache should be bypassed for requests whose lengths are unset. This
   * flag is provided for legacy reasons only.
   */
  public static final int FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS = 1 << 2; // 4

  /**
   * Reasons the cache may be ignored. One of {@link #CACHE_IGNORED_REASON_ERROR} or {@link
   * #CACHE_IGNORED_REASON_UNSET_LENGTH}.
   */
  @Documented
  @Retention(RetentionPolicy.SOURCE)
  @IntDef({CACHE_IGNORED_REASON_ERROR, CACHE_IGNORED_REASON_UNSET_LENGTH})
  public @interface CacheIgnoredReason {}

  /** Cache not ignored. */
  private static final int CACHE_NOT_IGNORED = -1;

  /** Cache ignored due to a cache related error. */
  public static final int CACHE_IGNORED_REASON_ERROR = 0;

  /** Cache ignored due to a request with an unset length. */
  public static final int CACHE_IGNORED_REASON_UNSET_LENGTH = 1;

  /** Minimum number of bytes to read before checking cache for availability. */
  private static final long MIN_READ_BEFORE_CHECKING_CACHE = 100 * 1024;

  private final Cache cache;
  private final DataSource cacheReadDataSource;
  @Nullable private final DataSource cacheWriteDataSource;
  private final DataSource upstreamDataSource;
  private final CacheKeyFactory cacheKeyFactory;
  @Nullable private final PriorityTaskManager upstreamPriorityTaskManager;
  private final int upstreamPriority;
  @Nullable private final EventListener eventListener;

  private final boolean blockOnCache;
  private final boolean ignoreCacheOnError;
  private final boolean ignoreCacheForUnsetLengthRequests;

  @Nullable private Uri actualUri;
  @Nullable private DataSpec requestDataSpec;
  @Nullable private DataSource currentDataSource;
  private boolean currentDataSpecLengthUnset;
  private long readPosition;
  private long bytesRemaining;
  @Nullable private CacheSpan currentHoleSpan;
  private boolean seenCacheError;
  private boolean currentRequestIgnoresCache;
  private long totalCachedBytesRead;
  private long checkCachePosition;

  /**
   * Constructs an instance with default {@link DataSource} and {@link DataSink} instances for
   * reading and writing the cache.
   *
   * @param cache The cache.
   * @param upstreamDataSource A {@link DataSource} for reading data not in the cache. If null,
   *     reading will fail if a cache miss occurs.
   */
  public CacheDataSource(Cache cache, @Nullable DataSource upstreamDataSource) {
    this(cache, upstreamDataSource, /* flags= */ 0);
  }

  /**
   * Constructs an instance with default {@link DataSource} and {@link DataSink} instances for
   * reading and writing the cache.
   *
   * @param cache The cache.
   * @param upstreamDataSource A {@link DataSource} for reading data not in the cache. If null,
   *     reading will fail if a cache miss occurs.
   * @param flags A combination of {@link #FLAG_BLOCK_ON_CACHE}, {@link #FLAG_IGNORE_CACHE_ON_ERROR}
   *     and {@link #FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS}, or 0.
   */
  public CacheDataSource(Cache cache, @Nullable DataSource upstreamDataSource, @Flags int flags) {
    this(
        cache,
        upstreamDataSource,
        new FileDataSource(),
        new CacheDataSink(cache, CacheDataSink.DEFAULT_FRAGMENT_SIZE),
        flags,
        /* eventListener= */ null);
  }

  /**
   * Constructs an instance with arbitrary {@link DataSource} and {@link DataSink} instances for
   * reading and writing the cache. One use of this constructor is to allow data to be transformed
   * before it is written to disk.
   *
   * @param cache The cache.
   * @param upstreamDataSource A {@link DataSource} for reading data not in the cache. If null,
   *     reading will fail if a cache miss occurs.
   * @param cacheReadDataSource A {@link DataSource} for reading data from the cache.
   * @param cacheWriteDataSink A {@link DataSink} for writing data to the cache. If null, cache is
   *     accessed read-only.
   * @param flags A combination of {@link #FLAG_BLOCK_ON_CACHE}, {@link #FLAG_IGNORE_CACHE_ON_ERROR}
   *     and {@link #FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS}, or 0.
   * @param eventListener An optional {@link EventListener} to receive events.
   */
  public CacheDataSource(
      Cache cache,
      @Nullable DataSource upstreamDataSource,
      DataSource cacheReadDataSource,
      @Nullable DataSink cacheWriteDataSink,
      @Flags int flags,
      @Nullable EventListener eventListener) {
    this(
        cache,
        upstreamDataSource,
        cacheReadDataSource,
        cacheWriteDataSink,
        flags,
        eventListener,
        /* cacheKeyFactory= */ null);
  }

  /**
   * Constructs an instance with arbitrary {@link DataSource} and {@link DataSink} instances for
   * reading and writing the cache. One use of this constructor is to allow data to be transformed
   * before it is written to disk.
   *
   * @param cache The cache.
   * @param upstreamDataSource A {@link DataSource} for reading data not in the cache. If null,
   *     reading will fail if a cache miss occurs.
   * @param cacheReadDataSource A {@link DataSource} for reading data from the cache.
   * @param cacheWriteDataSink A {@link DataSink} for writing data to the cache. If null, cache is
   *     accessed read-only.
   * @param flags A combination of {@link #FLAG_BLOCK_ON_CACHE}, {@link #FLAG_IGNORE_CACHE_ON_ERROR}
   *     and {@link #FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS}, or 0.
   * @param eventListener An optional {@link EventListener} to receive events.
   * @param cacheKeyFactory An optional factory for cache keys.
   */
  public CacheDataSource(
      Cache cache,
      @Nullable DataSource upstreamDataSource,
      DataSource cacheReadDataSource,
      @Nullable DataSink cacheWriteDataSink,
      @Flags int flags,
      @Nullable EventListener eventListener,
      @Nullable CacheKeyFactory cacheKeyFactory) {
    this(
        cache,
        upstreamDataSource,
        cacheReadDataSource,
        cacheWriteDataSink,
        cacheKeyFactory,
        flags,
        /* upstreamPriorityTaskManager= */ null,
        /* upstreamPriority= */ C.PRIORITY_PLAYBACK,
        eventListener);
  }

  private CacheDataSource(
      Cache cache,
      @Nullable DataSource upstreamDataSource,
      DataSource cacheReadDataSource,
      @Nullable DataSink cacheWriteDataSink,
      @Nullable CacheKeyFactory cacheKeyFactory,
      @Flags int flags,
      @Nullable PriorityTaskManager upstreamPriorityTaskManager,
      int upstreamPriority,
      @Nullable EventListener eventListener) {
    this.cache = cache;
    this.cacheReadDataSource = cacheReadDataSource;
    this.cacheKeyFactory =
        cacheKeyFactory != null ? cacheKeyFactory : CacheUtil.DEFAULT_CACHE_KEY_FACTORY;
    this.blockOnCache = (flags & FLAG_BLOCK_ON_CACHE) != 0;
    this.ignoreCacheOnError = (flags & FLAG_IGNORE_CACHE_ON_ERROR) != 0;
    this.ignoreCacheForUnsetLengthRequests =
        (flags & FLAG_IGNORE_CACHE_FOR_UNSET_LENGTH_REQUESTS) != 0;
    this.upstreamPriority = upstreamPriority;
    if (upstreamDataSource != null) {
      if (upstreamPriorityTaskManager != null) {
        upstreamDataSource =
            new PriorityDataSource(
                upstreamDataSource, upstreamPriorityTaskManager, upstreamPriority);
      }
      this.upstreamDataSource = upstreamDataSource;
      this.upstreamPriorityTaskManager = upstreamPriorityTaskManager;
      this.cacheWriteDataSource =
          cacheWriteDataSink != null
              ? new TeeDataSource(upstreamDataSource, cacheWriteDataSink)
              : null;
    } else {
      this.upstreamDataSource = DummyDataSource.INSTANCE;
      this.upstreamPriorityTaskManager = null;
      this.cacheWriteDataSource = null;
    }
    this.eventListener = eventListener;
  }

  /** Returns the {@link Cache} used by this instance. */
  public Cache getCache() {
    return cache;
  }

  /** Returns the {@link CacheKeyFactory} used by this instance. */
  public CacheKeyFactory getCacheKeyFactory() {
    return cacheKeyFactory;
  }

  /**
   * Returns the {@link PriorityTaskManager} used when there's a cache miss and requests need to be
   * made to the upstream {@link DataSource}, or {@code null} if there is none.
   */
  @Nullable
  public PriorityTaskManager getUpstreamPriorityTaskManager() {
    return upstreamPriorityTaskManager;
  }

  /**
   * Returns the priority used when there's a cache miss and requests need to be made to the
   * upstream {@link DataSource}. The priority is only used if the source has a {@link
   * PriorityTaskManager}.
   */
  public int getUpstreamPriority() {
    return upstreamPriority;
  }

  @Override
  public void addTransferListener(TransferListener transferListener) {
    cacheReadDataSource.addTransferListener(transferListener);
    upstreamDataSource.addTransferListener(transferListener);
  }

  @Override
  public long open(DataSpec dataSpec) throws IOException {
    try {
      String key = cacheKeyFactory.buildCacheKey(dataSpec);
      requestDataSpec = dataSpec.buildUpon().setKey(key).build();
      actualUri = getRedirectedUriOrDefault(cache, key, /* defaultUri= */ requestDataSpec.uri);
      readPosition = dataSpec.position;

      int reason = shouldIgnoreCacheForRequest(dataSpec);
      currentRequestIgnoresCache = reason != CACHE_NOT_IGNORED;
      if (currentRequestIgnoresCache) {
        notifyCacheIgnored(reason);
      }

      if (dataSpec.length != C.LENGTH_UNSET || currentRequestIgnoresCache) {
        bytesRemaining = dataSpec.length;
      } else {
        bytesRemaining = ContentMetadata.getContentLength(cache.getContentMetadata(key));
        if (bytesRemaining != C.LENGTH_UNSET) {
          bytesRemaining -= dataSpec.position;
          if (bytesRemaining <= 0) {
            throw new DataSourceException(DataSourceException.POSITION_OUT_OF_RANGE);
          }
        }
      }
      openNextSource(false);
      return bytesRemaining;
    } catch (Throwable e) {
      handleBeforeThrow(e);
      throw e;
    }
  }

  @Override
  public int read(byte[] buffer, int offset, int readLength) throws IOException {
    if (readLength == 0) {
      return 0;
    }
    if (bytesRemaining == 0) {
      return C.RESULT_END_OF_INPUT;
    }
    try {
      if (readPosition >= checkCachePosition) {
        openNextSource(true);
      }
      int bytesRead = currentDataSource.read(buffer, offset, readLength);
      if (bytesRead != C.RESULT_END_OF_INPUT) {
        if (isReadingFromCache()) {
          totalCachedBytesRead += bytesRead;
        }
        readPosition += bytesRead;
        if (bytesRemaining != C.LENGTH_UNSET) {
          bytesRemaining -= bytesRead;
        }
      } else if (currentDataSpecLengthUnset) {
        setNoBytesRemainingAndMaybeStoreLength();
      } else if (bytesRemaining > 0 || bytesRemaining == C.LENGTH_UNSET) {
        closeCurrentSource();
        openNextSource(false);
        return read(buffer, offset, readLength);
      }
      return bytesRead;
    } catch (IOException e) {
      if (currentDataSpecLengthUnset && CacheUtil.isCausedByPositionOutOfRange(e)) {
        setNoBytesRemainingAndMaybeStoreLength();
        return C.RESULT_END_OF_INPUT;
      }
      handleBeforeThrow(e);
      throw e;
    } catch (Throwable e) {
      handleBeforeThrow(e);
      throw e;
    }
  }

  @Override
  @Nullable
  public Uri getUri() {
    return actualUri;
  }

  @Override
  public Map<String, List<String>> getResponseHeaders() {
    // TODO: Implement.
    return isReadingFromUpstream()
        ? upstreamDataSource.getResponseHeaders()
        : Collections.emptyMap();
  }

  @Override
  public void close() throws IOException {
    requestDataSpec = null;
    actualUri = null;
    readPosition = 0;
    notifyBytesRead();
    try {
      closeCurrentSource();
    } catch (Throwable e) {
      handleBeforeThrow(e);
      throw e;
    }
  }

  /**
   * Opens the next source. If the cache contains data spanning the current read position then
   * {@link #cacheReadDataSource} is opened to read from it. Else {@link #upstreamDataSource} is
   * opened to read from the upstream source and write into the cache.
   *
   * <p>There must not be a currently open source when this method is called, except in the case
   * that {@code checkCache} is true. If {@code checkCache} is true then there must be a currently
   * open source, and it must be {@link #upstreamDataSource}. It will be closed and a new source
   * opened if it's possible to switch to reading from or writing to the cache. If a switch isn't
   * possible then the current source is left unchanged.
   *
   * @param checkCache If true tries to switch to reading from or writing to cache instead of
   *     reading from {@link #upstreamDataSource}, which is the currently open source.
   */
  private void openNextSource(boolean checkCache) throws IOException {
    @Nullable CacheSpan nextSpan;
    String key = requestDataSpec.key;
    if (currentRequestIgnoresCache) {
      nextSpan = null;
    } else if (blockOnCache) {
      try {
        nextSpan = cache.startReadWrite(key, readPosition);
      } catch (InterruptedException e) {
        Thread.currentThread().interrupt();
        throw new InterruptedIOException();
      }
    } else {
      nextSpan = cache.startReadWriteNonBlocking(key, readPosition);
    }

    DataSpec nextDataSpec;
    DataSource nextDataSource;
    if (nextSpan == null) {
      // The data is locked in the cache, or we're ignoring the cache. Bypass the cache and read
      // from upstream.
      nextDataSource = upstreamDataSource;
      nextDataSpec =
          requestDataSpec.buildUpon().setPosition(readPosition).setLength(bytesRemaining).build();
    } else if (nextSpan.isCached) {
      // Data is cached in a span file starting at nextSpan.position.
      Uri fileUri = Uri.fromFile(nextSpan.file);
      long filePositionOffset = nextSpan.position;
      long positionInFile = readPosition - filePositionOffset;
      long length = nextSpan.length - positionInFile;
      if (bytesRemaining != C.LENGTH_UNSET) {
        length = Math.min(length, bytesRemaining);
      }
      nextDataSpec =
          requestDataSpec
              .buildUpon()
              .setUri(fileUri)
              .setUriPositionOffset(filePositionOffset)
              .setPosition(positionInFile)
              .setLength(length)
              .build();
      nextDataSource = cacheReadDataSource;
    } else {
      // Data is not cached, and data is not locked, read from upstream with cache backing.
      long length;
      if (nextSpan.isOpenEnded()) {
        length = bytesRemaining;
      } else {
        length = nextSpan.length;
        if (bytesRemaining != C.LENGTH_UNSET) {
          length = Math.min(length, bytesRemaining);
        }
      }
      nextDataSpec =
          requestDataSpec.buildUpon().setPosition(readPosition).setLength(length).build();
      if (cacheWriteDataSource != null) {
        nextDataSource = cacheWriteDataSource;
      } else {
        nextDataSource = upstreamDataSource;
        cache.releaseHoleSpan(nextSpan);
        nextSpan = null;
      }
    }

    checkCachePosition =
        !currentRequestIgnoresCache && nextDataSource == upstreamDataSource
            ? readPosition + MIN_READ_BEFORE_CHECKING_CACHE
            : Long.MAX_VALUE;
    if (checkCache) {
      Assertions.checkState(isBypassingCache());
      if (nextDataSource == upstreamDataSource) {
        // Continue reading from upstream.
        return;
      }
      // We're switching to reading from or writing to the cache.
      try {
        closeCurrentSource();
      } catch (Throwable e) {
        if (nextSpan.isHoleSpan()) {
          // Release the hole span before throwing, else we'll hold it forever.
          cache.releaseHoleSpan(nextSpan);
        }
        throw e;
      }
    }

    if (nextSpan != null && nextSpan.isHoleSpan()) {
      currentHoleSpan = nextSpan;
    }
    currentDataSource = nextDataSource;
    currentDataSpecLengthUnset = nextDataSpec.length == C.LENGTH_UNSET;
    long resolvedLength = nextDataSource.open(nextDataSpec);

    // Update bytesRemaining, actualUri and (if writing to cache) the cache metadata.
    ContentMetadataMutations mutations = new ContentMetadataMutations();
    if (currentDataSpecLengthUnset && resolvedLength != C.LENGTH_UNSET) {
      bytesRemaining = resolvedLength;
      ContentMetadataMutations.setContentLength(mutations, readPosition + bytesRemaining);
    }
    if (isReadingFromUpstream()) {
      actualUri = currentDataSource.getUri();
      boolean isRedirected = !requestDataSpec.uri.equals(actualUri);
      ContentMetadataMutations.setRedirectedUri(mutations, isRedirected ? actualUri : null);
    }
    if (isWritingToCache()) {
      cache.applyContentMetadataMutations(key, mutations);
    }
  }

  private void setNoBytesRemainingAndMaybeStoreLength() throws IOException {
    bytesRemaining = 0;
    if (isWritingToCache()) {
      ContentMetadataMutations mutations = new ContentMetadataMutations();
      ContentMetadataMutations.setContentLength(mutations, readPosition);
      cache.applyContentMetadataMutations(requestDataSpec.key, mutations);
    }
  }

  private static Uri getRedirectedUriOrDefault(Cache cache, String key, Uri defaultUri) {
    @Nullable Uri redirectedUri = ContentMetadata.getRedirectedUri(cache.getContentMetadata(key));
    return redirectedUri != null ? redirectedUri : defaultUri;
  }

  private boolean isReadingFromUpstream() {
    return !isReadingFromCache();
  }

  private boolean isBypassingCache() {
    return currentDataSource == upstreamDataSource;
  }

  private boolean isReadingFromCache() {
    return currentDataSource == cacheReadDataSource;
  }

  private boolean isWritingToCache() {
    return currentDataSource == cacheWriteDataSource;
  }

  private void closeCurrentSource() throws IOException {
    if (currentDataSource == null) {
      return;
    }
    try {
      currentDataSource.close();
    } finally {
      currentDataSource = null;
      currentDataSpecLengthUnset = false;
      if (currentHoleSpan != null) {
        cache.releaseHoleSpan(currentHoleSpan);
        currentHoleSpan = null;
      }
    }
  }

  private void handleBeforeThrow(Throwable exception) {
    if (isReadingFromCache() || exception instanceof CacheException) {
      seenCacheError = true;
    }
  }

  private int shouldIgnoreCacheForRequest(DataSpec dataSpec) {
    if (ignoreCacheOnError && seenCacheError) {
      return CACHE_IGNORED_REASON_ERROR;
    } else if (ignoreCacheForUnsetLengthRequests && dataSpec.length == C.LENGTH_UNSET) {
      return CACHE_IGNORED_REASON_UNSET_LENGTH;
    } else {
      return CACHE_NOT_IGNORED;
    }
  }

  private void notifyCacheIgnored(@CacheIgnoredReason int reason) {
    if (eventListener != null) {
      eventListener.onCacheIgnored(reason);
    }
  }

  private void notifyBytesRead() {
    if (eventListener != null && totalCachedBytesRead > 0) {
      eventListener.onCachedBytesRead(cache.getCacheSpace(), totalCachedBytesRead);
      totalCachedBytesRead = 0;
    }
  }

}
