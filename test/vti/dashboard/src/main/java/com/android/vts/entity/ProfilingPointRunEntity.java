/*
 * Copyright (c) 2017 Google Inc. All Rights Reserved.
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

package com.android.vts.entity;

import com.android.vts.proto.VtsReportMessage.ProfilingReportMessage;
import com.android.vts.proto.VtsReportMessage.VtsProfilingRegressionMode;
import com.android.vts.proto.VtsReportMessage.VtsProfilingType;
import com.google.appengine.api.datastore.Entity;
import com.google.appengine.api.datastore.Key;
import com.google.appengine.api.datastore.KeyFactory;
import com.google.common.collect.Lists;
import com.google.protobuf.ByteString;
import com.googlecode.objectify.annotation.Cache;
import com.googlecode.objectify.annotation.Id;
import com.googlecode.objectify.annotation.Ignore;
import com.googlecode.objectify.annotation.Index;
import com.googlecode.objectify.annotation.Parent;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Objects;
import lombok.Data;
import lombok.NoArgsConstructor;
import lombok.extern.log4j.Log4j2;

import static com.googlecode.objectify.ObjectifyService.ofy;

@com.googlecode.objectify.annotation.Entity(name = "ProfilingPointRun")
@Cache
@Data
@NoArgsConstructor
@Log4j2
/** Entity describing a profiling point execution. */
public class ProfilingPointRunEntity implements DashboardEntity {

    public static final String KIND = "ProfilingPointRun";

    // Property keys
    public static final String TYPE = "type";
    public static final String REGRESSION_MODE = "regressionMode";
    public static final String LABELS = "labels";
    public static final String VALUES = "values";
    public static final String X_LABEL = "xLabel";
    public static final String Y_LABEL = "yLabel";
    public static final String OPTIONS = "options";

    /** This value will set the limit size of values array field */
    public static final int VALUE_SIZE_LIMIT = 50000;

    @Ignore
    private Key key;

    /** ID field using profilingPointName */
    @Id
    private String name;

    /** parent field based on Test and TestRun key */
    @Parent
    private com.googlecode.objectify.Key<?> parent;

    /** VtsProfilingType in ProfilingPointRunEntity class  */
    private int type;

    /** VtsProfilingType in ProfilingPointRunEntity class  */
    private int regressionMode;

    /** list of label name  */
    private List<String> labels;

    /** list of values  */
    private List<Long> values;

    /** X axis label name */
    private String xLabel;

    /** Y axis label name */
    private String yLabel;

    /** Test Suite file name field */
    private List<String> options;

    /** When this record was created or updated */
    @Index Date updated;

    /**
     * Create a ProfilingPointRunEntity object.
     *
     * @param parentKey The Key object for the parent TestRunEntity in datastore.
     * @param name The name of the profiling point.
     * @param type The (number) type of the profiling point data.
     * @param regressionMode The (number) mode to use for detecting regression.
     * @param labels List of data labels, or null if the data is unlabeled.
     * @param values List of data values.
     * @param xLabel The x axis label.
     * @param yLabel The y axis label.
     * @param options The list of key=value options for the profiling point run.
     */
    public ProfilingPointRunEntity(
            Key parentKey,
            String name,
            int type,
            int regressionMode,
            List<String> labels,
            List<Long> values,
            String xLabel,
            String yLabel,
            List<String> options) {
        this.key = KeyFactory.createKey(parentKey, KIND, name);
        this.name = name;
        this.type = type;
        this.regressionMode = regressionMode;
        this.labels = labels == null ? null : new ArrayList<>(labels);
        this.values = new ArrayList<>(values);
        this.xLabel = xLabel;
        this.yLabel = yLabel;
        this.options = options;
        this.updated = new Date();
    }


    /**
     * Create a ProfilingPointRunEntity object.
     *
     * @param parent The objectify Key for the parent TestRunEntity in datastore.
     * @param name The name of the profiling point.
     * @param type The (number) type of the profiling point data.
     * @param regressionMode The (number) mode to use for detecting regression.
     * @param labels List of data labels, or null if the data is unlabeled.
     * @param values List of data values.
     * @param xLabel The x axis label.
     * @param yLabel The y axis label.
     * @param options The list of key=value options for the profiling point run.
     */
    public ProfilingPointRunEntity(
            com.googlecode.objectify.Key parent,
            String name,
            int type,
            int regressionMode,
            List<String> labels,
            List<Long> values,
            String xLabel,
            String yLabel,
            List<String> options) {
        this.parent = parent;
        this.name = name;
        this.type = type;
        this.regressionMode = regressionMode;
        this.labels = labels == null ? null : new ArrayList<>(labels);
        this.values = new ArrayList<>(values);
        this.xLabel = xLabel;
        this.yLabel = yLabel;
        this.options = options;
        this.updated = new Date();
    }

    /**
     * Get VtsProfilingType from int value.
     *
     * @return VtsProfilingType class.
     */
    public VtsProfilingType getVtsProfilingType(int type) {
        return VtsProfilingType.forNumber(type);
    }

    /**
     * Get VtsProfilingRegressionMode from int value.
     *
     * @return VtsProfilingType class.
     */
    public VtsProfilingRegressionMode getVtsProfilingRegressionMode(int regressionMode) {
        return VtsProfilingRegressionMode.forNumber(regressionMode);
    }

    /**
     * Save multi rows function when the record exceed the limit which is 1MB.
     *
     * @return ProfilingPointRunEntity's key value.
     */
    public com.googlecode.objectify.Key<ProfilingPointRunEntity> saveMultiRow() {
        if (this.getValues().size() > VALUE_SIZE_LIMIT) {

            List<List<Long>> partitionedValueList =
                    Lists.partition(this.getValues(), VALUE_SIZE_LIMIT);
            int partitionedValueListSize = partitionedValueList.size();

            List<List<String>> partitionedLabelList = new ArrayList<>();
            if (Objects.nonNull(this.getLabels()) && this.getLabels().size() > VALUE_SIZE_LIMIT) {
                partitionedLabelList = Lists.partition(this.getLabels(), VALUE_SIZE_LIMIT);
            }

            com.googlecode.objectify.Key<ProfilingPointRunEntity> profilingPointRunEntityKey = null;
            if (partitionedValueListSize < VALUE_SIZE_LIMIT) {
                for (int index = 0; index < partitionedValueListSize; index++) {
                    if (index > 0) {
                        this.values.addAll(partitionedValueList.get(index));
                        if (index < partitionedLabelList.size()) {
                            this.labels.addAll(partitionedLabelList.get(index));
                        }
                    } else {
                        this.values = partitionedValueList.get(index);
                        if (index < partitionedLabelList.size()) {
                            this.labels = partitionedLabelList.get(index);
                        }
                    }
                    profilingPointRunEntityKey = ofy().save().entity(this).now();
                }
            }
            return profilingPointRunEntityKey;
        } else {
            return ofy().save().entity(this).now();
        }
    }

    /** Saving function for the instance of this class */
    @Override
    public com.googlecode.objectify.Key<ProfilingPointRunEntity> save() {
        return ofy().save().entity(this).now();
    }

    public Entity toEntity() {
        Entity profilingRun = new Entity(this.key);
        profilingRun.setUnindexedProperty(TYPE, this.type);
        profilingRun.setUnindexedProperty(REGRESSION_MODE, this.regressionMode);
        if (this.labels != null) {
            profilingRun.setUnindexedProperty(LABELS, this.labels);
        }
        profilingRun.setUnindexedProperty(VALUES, this.values);
        profilingRun.setUnindexedProperty(X_LABEL, this.xLabel);
        profilingRun.setUnindexedProperty(Y_LABEL, this.yLabel);
        if (this.options != null) {
            profilingRun.setUnindexedProperty(OPTIONS, this.options);
        }

        return profilingRun;
    }

    /**
     * Convert an Entity object to a ProflilingPointRunEntity.
     *
     * @param e The entity to process.
     * @return ProfilingPointRunEntity object with the properties from e, or null if incompatible.
     */
    @SuppressWarnings("unchecked")
    public static ProfilingPointRunEntity fromEntity(Entity e) {
        if (!e.getKind().equals(KIND)
                || e.getKey().getName() == null
                || !e.hasProperty(TYPE)
                || !e.hasProperty(REGRESSION_MODE)
                || !e.hasProperty(VALUES)
                || !e.hasProperty(X_LABEL)
                || !e.hasProperty(Y_LABEL)) {
            log.error("Missing profiling point attributes in entity: " + e.toString());
            return null;
        }
        try {
            Key parentKey = e.getParent();
            String name = e.getKey().getName();
            int type = (int) (long) e.getProperty(TYPE);
            int regressionMode = (int) (long) e.getProperty(REGRESSION_MODE);
            List<Long> values = (List<Long>) e.getProperty(VALUES);
            String xLabel = (String) e.getProperty(X_LABEL);
            String yLabel = (String) e.getProperty(Y_LABEL);
            List<String> labels = null;
            if (e.hasProperty(LABELS)) {
                labels = (List<String>) e.getProperty(LABELS);
            }
            List<String> options = null;
            if (e.hasProperty(OPTIONS)) {
                options = (List<String>) e.getProperty(OPTIONS);
            }
            return new ProfilingPointRunEntity(
                    parentKey, name, type, regressionMode, labels, values, xLabel, yLabel, options);
        } catch (ClassCastException exception) {
            // Invalid cast
            log.warn("Error parsing profiling point run entity.", exception);
        }
        return null;
    }

    /**
     * Convert a coverage report to a CoverageEntity.
     *
     * @param parent The ancestor objectify key for the coverage entity.
     * @param profilingReport The profiling report containing profiling data.
     * @return The ProfilingPointRunEntity for the profiling report message, or null if incompatible
     */
    public static ProfilingPointRunEntity fromProfilingReport(
            com.googlecode.objectify.Key parent, ProfilingReportMessage profilingReport) {
        if (!profilingReport.hasName()
                || !profilingReport.hasType()
                || profilingReport.getType() == VtsProfilingType.UNKNOWN_VTS_PROFILING_TYPE
                || !profilingReport.hasRegressionMode()
                || !profilingReport.hasXAxisLabel()
                || !profilingReport.hasYAxisLabel()) {
            return null; // invalid profiling report;
        }
        String name = profilingReport.getName().toStringUtf8();
        VtsProfilingType type = profilingReport.getType();
        VtsProfilingRegressionMode regressionMode = profilingReport.getRegressionMode();
        String xLabel = profilingReport.getXAxisLabel().toStringUtf8();
        String yLabel = profilingReport.getYAxisLabel().toStringUtf8();
        List<Long> values;
        List<String> labels = null;
        switch (type) {
            case VTS_PROFILING_TYPE_TIMESTAMP:
                if (!profilingReport.hasStartTimestamp()
                        || !profilingReport.hasEndTimestamp()
                        || profilingReport.getEndTimestamp()
                                < profilingReport.getStartTimestamp()) {
                    return null; // missing timestamp
                }
                long value =
                        profilingReport.getEndTimestamp() - profilingReport.getStartTimestamp();
                values = new ArrayList<>();
                values.add(value);
                break;
            case VTS_PROFILING_TYPE_LABELED_VECTOR:
                if (profilingReport.getValueCount() != profilingReport.getLabelCount()) {
                    return null; // jagged data
                }
                labels = new ArrayList<>();
                for (ByteString label : profilingReport.getLabelList()) {
                    labels.add(label.toStringUtf8());
                }
                values = profilingReport.getValueList();
                break;
            case VTS_PROFILING_TYPE_UNLABELED_VECTOR:
                values = profilingReport.getValueList();
                break;
            default: // should never happen
                return null;
        }
        if (values.size() > VALUE_SIZE_LIMIT) {
            values = values.subList(0, VALUE_SIZE_LIMIT);
        }
        List<String> options = null;
        if (profilingReport.getOptionsCount() > 0) {
            options = new ArrayList<>();
            for (ByteString optionBytes : profilingReport.getOptionsList()) {
                options.add(optionBytes.toStringUtf8());
            }
        }
        return new ProfilingPointRunEntity(
                parent,
                name,
                type.getNumber(),
                regressionMode.getNumber(),
                labels,
                values,
                xLabel,
                yLabel,
                options);
    }
}
