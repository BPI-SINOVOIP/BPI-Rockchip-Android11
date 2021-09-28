/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.vts.config;

import com.googlecode.objectify.ObjectifyFilter;

import javax.servlet.annotation.WebFilter;

/** Annotation used to declare a servlet filter. */
@WebFilter(urlPatterns = {"/*"})
/**
 * Servlet filter for objectify library. Objectify requires a filter to clean up any thread-local
 * transaction contexts and pending asynchronous operations that remain at the end of a request.
 */
public class ObjectifyWebFilter extends ObjectifyFilter {}
