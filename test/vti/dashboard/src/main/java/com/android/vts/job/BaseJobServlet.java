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

package com.android.vts.job;


import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.http.HttpServlet;
import java.util.Properties;

/**
 * An abstract class to be subclassed to create Job Servlet
 */
public abstract class BaseJobServlet extends HttpServlet {

    /**
     * System Configuration Property class
     */
    protected static Properties systemConfigProp = new Properties();

    /**
     * This variable is for maximum number of entities per transaction You can find the detail here
     * (https://cloud.google.com/datastore/docs/concepts/limits)
     */
    protected int MAX_ENTITY_SIZE_PER_TRANSACTION = 300;

    @Override
    public void init(ServletConfig cfg) throws ServletException {
        super.init(cfg);

        systemConfigProp =
                Properties.class.cast(cfg.getServletContext().getAttribute("systemConfigProp"));

        this.MAX_ENTITY_SIZE_PER_TRANSACTION =
                Integer.parseInt(systemConfigProp.getProperty("datastore.maxEntitySize"));
    }
}
