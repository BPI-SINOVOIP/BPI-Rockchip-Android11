/*
 * Copyright (c) 2018 Google Inc. All Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you
 * may not use this file except in compliance with the License. You may
 * obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.vts.util;

import com.google.cloud.datastore.Cursor;
import com.google.cloud.datastore.QueryResults;
import com.googlecode.objectify.cmd.Query;

import java.util.ArrayList;
import java.util.Iterator;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/** Helper class for pagination. */
public class Pagination<T> implements Iterable<T> {

    /** the default page size */
    public static final int DEFAULT_PAGE_SIZE = 10;
    /** the default page window size */
    private static final int DEFAULT_PAGE_WINDOW = 10;

    /** the current page */
    private int page;

    /** the page window size */
    private int pageSize = DEFAULT_PAGE_SIZE;

    /** the total number of found entities */
    private int totalCount;

    /** the previous cursor string token where to start */
    private String previousPageCountToken = "";

    /** the previous cursor string token where to start */
    private String currentPageCountToken = "";

    /** the next cursor string token where to start */
    private String nextPageCountToken = "";

    /** the previous cursor string token list where to start */
    private LinkedHashSet<String> pageCountTokenSet;

    /** the maximum number of pages */
    private int maxPages;

    /** the list of object on the page */
    private List<T> list = new ArrayList<>();

    public Pagination(List<T> list, int page, int pageSize, int totalCount) {
        this.list = list;
        this.page = page;
        this.pageSize = pageSize;
        this.totalCount = totalCount;
    }

    public Pagination(
            Query<T> query,
            int page,
            int pageSize,
            String startPageToken,
            LinkedHashSet<String> pageCountTokenSet) {
        this.page = page;
        this.pageSize = pageSize;
        this.pageCountTokenSet = pageCountTokenSet;
        this.currentPageCountToken = startPageToken;

        List<String> pageCountTokenList =
                this.pageCountTokenSet.stream().collect(Collectors.toList());
        if (pageCountTokenList.size() > 0) {
            int foundIndex = pageCountTokenList.indexOf(startPageToken);
            if (foundIndex <= 0) {
                this.previousPageCountToken = "";
            } else {
                this.previousPageCountToken = pageCountTokenList.get(foundIndex - 1);
            }
        }

        int limitValue = pageSize * DEFAULT_PAGE_WINDOW + pageSize / 2;
        query = query.limit(limitValue);
        if (startPageToken.equals("")) {
            this.totalCount = query.count();
        } else {
            query = query.startAt(Cursor.fromUrlSafe(startPageToken));
            this.totalCount = query.count();
        }

        this.maxPages =
                this.totalCount / this.pageSize + (this.totalCount % this.pageSize == 0 ? 0 : 1);

        int iteratorIndex = 0;
        int startIndex = (page % pageSize == 0 ? 9 : page % pageSize - 1) * pageSize;

        QueryResults<T> resultIterator = query.iterator();
        while (resultIterator.hasNext()) {
            if (startIndex <= iteratorIndex && iteratorIndex < startIndex + this.pageSize)
                this.list.add(resultIterator.next());
            else resultIterator.next();
            if (iteratorIndex == DEFAULT_PAGE_WINDOW * this.pageSize) {
                if ( Objects.nonNull(resultIterator) && Objects.nonNull(resultIterator.getCursorAfter()) ) {
                    this.nextPageCountToken = resultIterator.getCursorAfter().toUrlSafe();
                }
            }
            iteratorIndex++;
        }
    }

    public Iterator<T> iterator() {
        return list.iterator();
    }

    /**
     * Gets the total number of objects.
     *
     * @return the total number of objects as an int
     */
    public int getTotalCount() {
        return totalCount;
    }

    /**
     * Gets the number of page window.
     *
     * @return the number of page window as an int
     */
    public int getPageSize() {
        return pageSize;
    }

    /**
     * Gets the maximum number of pages.
     *
     * @return the maximum number of pages as an int
     */
    public int getMaxPages() {
        return this.maxPages;
    }

    /**
     * Gets the page.
     *
     * @return the page as an int
     */
    public int getPage() {
        return page;
    }

    /**
     * Gets the minimum page in the window.
     *
     * @return the page number
     */
    public int getMinPageRange() {
        if (this.getPage() <= this.getPageSize()) {
            return 1;
        } else {
            if ((this.getPage() % this.getPageSize()) == 0) {
                return (this.getPage() / this.getPageSize() - 1) * this.getPageSize() + 1;
            } else {
                return this.getPage() / this.getPageSize() * this.getPageSize() + 1;
            }
        }
    }

    /**
     * Gets the maximum page in the window.
     *
     * @return the page number
     */
    public int getMaxPageRange() {
        if (this.getMaxPages() > this.getPageSize()) {
            return getMinPageRange() + this.getPageSize() - 1;
        } else {
            return this.getMinPageRange() + this.getMaxPages() - 1;
        }
    }

    /**
     * Gets the subset of the list for the current page.
     *
     * @return a List
     */
    public List<T> getList() {
        return this.list;
    }

    /**
     * Gets the cursor token for the previous page starting point.
     *
     * @return a string of cursor of previous starting point
     */
    public String getPreviousPageCountToken() {
        return this.previousPageCountToken;
    }

    /**
     * Gets the cursor token for the current page starting point.
     *
     * @return a string of cursor of current starting point
     */
    public String getCurrentPageCountToken() {
        return this.currentPageCountToken;
    }

    /**
     * Gets the cursor token for the next page starting point.
     *
     * @return a string of cursor of next starting point
     */
    public String getNextPageCountToken() {
        return this.nextPageCountToken;
    }
}
