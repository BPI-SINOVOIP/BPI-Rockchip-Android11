/*
 * Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

package org.apache.bcel.data;

@CombinedAnnotation( { @SimpleAnnotation(id = 4) })
public class AnnotatedWithCombinedAnnotation
{
    public AnnotatedWithCombinedAnnotation(final int param1, @SimpleAnnotation(id=42) final int param2) {
    }

    @CombinedAnnotation( {})
    public void methodWithArrayOfZeroAnnotations() {
    }

    @CombinedAnnotation( { @SimpleAnnotation(id=1, fruit="apples"), @SimpleAnnotation(id= 2, fruit="oranges")})
    public void methodWithArrayOfTwoAnnotations() {
    }
}
