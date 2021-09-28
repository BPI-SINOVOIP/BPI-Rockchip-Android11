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

package com.android.server.wm.flicker;

import androidx.annotation.Nullable;

import android.graphics.Rect;
import android.surfaceflinger.nano.Layers.LayerProto;
import android.surfaceflinger.nano.Layers.RectProto;
import android.surfaceflinger.nano.Layers.RegionProto;
import android.surfaceflinger.nano.Layerstrace.LayersTraceFileProto;
import android.surfaceflinger.nano.Layerstrace.LayersTraceProto;
import android.util.SparseArray;

import com.android.server.wm.flicker.Assertions.Result;

import java.nio.file.Path;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.Optional;
import java.util.function.Consumer;
import java.util.stream.Collectors;

/**
 * Contains a collection of parsed Layers trace entries and assertions to apply over a single entry.
 *
 * <p>Each entry is parsed into a list of {@link LayersTrace.Entry} objects.
 */
public class LayersTrace {
    private final List<Entry> mEntries;
    @Nullable private final Path mSource;

    private LayersTrace(List<Entry> entries, Path source) {
        this.mEntries = entries;
        this.mSource = source;
    }

    /**
     * Parses {@code LayersTraceFileProto} from {@code data} and uses the proto to generates a list
     * of trace entries, storing the flattened layers into its hierarchical structure.
     *
     * @param data binary proto data
     * @param source Path to source of data for additional debug information
     * @param orphanLayerCallback a callback to handle any unexpected orphan layers
     */
    public static LayersTrace parseFrom(byte[] data, Path source,
            Consumer<Layer> orphanLayerCallback) {
        List<Entry> entries = new ArrayList<>();
        LayersTraceFileProto fileProto;
        try {
            fileProto = LayersTraceFileProto.parseFrom(data);
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
        for (LayersTraceProto traceProto : fileProto.entry) {
            Entry entry =
                    Entry.fromFlattenedLayers(
                            traceProto.elapsedRealtimeNanos, traceProto.layers.layers,
                            orphanLayerCallback);
            entries.add(entry);
        }
        return new LayersTrace(entries, source);
    }

    /**
     * Parses {@code LayersTraceFileProto} from {@code data} and uses the proto to generates a list
     * of trace entries, storing the flattened layers into its hierarchical structure.
     *
     * @param data binary proto data
     * @param source Path to source of data for additional debug information
     */
    public static LayersTrace parseFrom(byte[] data, Path source) {
        return parseFrom(data, source, null /* orphanLayerCallback */);
    }

    /**
     * Parses {@code LayersTraceFileProto} from {@code data} and uses the proto to generates a list
     * of trace entries, storing the flattened layers into its hierarchical structure.
     *
     * @param data binary proto data
     */
    public static LayersTrace parseFrom(byte[] data) {
        return parseFrom(data, null);
    }

    public List<Entry> getEntries() {
        return mEntries;
    }

    public Entry getEntry(long timestamp) {
        Optional<Entry> entry =
                mEntries.stream().filter(e -> e.getTimestamp() == timestamp).findFirst();
        if (!entry.isPresent()) {
            throw new RuntimeException("Entry does not exist for timestamp " + timestamp);
        }
        return entry.get();
    }

    public Optional<Path> getSource() {
        return Optional.ofNullable(mSource);
    }

    /** Represents a single Layer trace entry. */
    public static class Entry implements ITraceEntry {
        private long mTimestamp;
        private List<Layer> mRootLayers; // hierarchical representation of layers
        private List<Layer> mFlattenedLayers = null;

        private Entry(long timestamp, List<Layer> rootLayers) {
            this.mTimestamp = timestamp;
            this.mRootLayers = rootLayers;
        }

        /**
         * Determines the id of the root element.
         *
         * <p>On some files, such as the ones used in the FlickerLib testdata, the root nodes are
         * those that have parent=0, on newer traces, the root nodes are those that have parent=-1
         *
         * <p>This function keeps compatibility with both new and older traces by searching for a
         * known root layer (Display Root) and considering its parent Id as overall root.
         */
        private static Layer getRootLayer(SparseArray<Layer> layerMap) {
            Layer knownRoot = null;
            int numKeys = layerMap.size();
            for (int i = 0; i < numKeys; ++i) {
                Layer currentLayer = layerMap.valueAt(i);
                if (currentLayer.isRootLayer()) {
                    knownRoot = currentLayer;
                    break;
                }
            }

            if (knownRoot == null) {
                throw new IllegalStateException("Display root layer not found.");
            }

            return layerMap.get(knownRoot.getParentId());
        }

        /** Constructs the layer hierarchy from a flattened list of layers. */
        public static Entry fromFlattenedLayers(long timestamp, LayerProto[] protos,
                Consumer<Layer> orphanLayerCallback) {
            SparseArray<Layer> layerMap = new SparseArray<>();
            ArrayList<Layer> orphans = new ArrayList<>();
            for (LayerProto proto : protos) {
                int id = proto.id;
                int parentId = proto.parent;

                Layer newLayer = layerMap.get(id);
                if (newLayer == null) {
                    newLayer = new Layer(proto);
                    layerMap.append(id, newLayer);
                } else if (newLayer.mProto != null) {
                    throw new RuntimeException("Duplicate layer id found:" + id);
                } else {
                    newLayer.mProto = proto;
                    orphans.remove(newLayer);
                }

                // add parent placeholder
                if (layerMap.get(parentId) == null) {
                    Layer orphanLayer = new Layer(null);
                    layerMap.append(parentId, orphanLayer);
                    orphans.add(orphanLayer);
                }
                layerMap.get(parentId).addChild(newLayer);
                newLayer.addParent(layerMap.get(parentId));
            }

            // Remove root node
            Layer rootLayer = getRootLayer(layerMap);
            orphans.remove(rootLayer);
            // Fail if we find orphan layers.
            orphans.forEach(
                    orphan -> {
                        if (orphanLayerCallback != null) {
                            // Workaround for b/141326137, ignore the existence of an orphan layer
                            orphanLayerCallback.accept(orphan);
                            return;
                        }
                        String childNodes =
                                orphan.mChildren
                                        .stream()
                                        .map(node -> Integer.toString(node.getId()))
                                        .collect(Collectors.joining(", "));
                        int orphanId = orphan.mChildren.get(0).getParentId();
                        throw new RuntimeException(
                                "Failed to parse layers trace. Found orphan layers with parent "
                                        + "layer id:"
                                        + orphanId
                                        + " : "
                                        + childNodes);
                    });

            return new Entry(timestamp, rootLayer.mChildren);
        }

        /** Extracts {@link Rect} from {@link RectProto}. */
        private static Rect extract(RectProto proto) {
            return new Rect(proto.left, proto.top, proto.right, proto.bottom);
        }

        /**
         * Extracts {@link Rect} from {@link RegionProto} by returning a rect that encompasses all
         * the rects making up the region.
         */
        private static Rect extract(RegionProto regionProto) {
            Rect region = new Rect();
            for (RectProto proto : regionProto.rect) {
                region.union(proto.left, proto.top, proto.right, proto.bottom);
            }
            return region;
        }

        /** Checks if a region specified by {@code testRect} is covered by all visible layers. */
        public Result coversRegion(Rect testRect) {
            String assertionName = "coversRegion";
            Collection<Layer> layers = asFlattenedLayers();

            for (int x = testRect.left; x < testRect.right; x++) {
                for (int y = testRect.top; y < testRect.bottom; y++) {
                    boolean emptyRegionFound = true;
                    for (Layer layer : layers) {
                        if (layer.isInvisible() || layer.isHiddenByParent()) {
                            continue;
                        }
                        for (RectProto rectProto : layer.mProto.visibleRegion.rect) {
                            Rect r = extract(rectProto);
                            if (r.contains(x, y)) {
                                y = r.bottom;
                                emptyRegionFound = false;
                            }
                        }
                    }
                    if (emptyRegionFound) {
                        String reason =
                                "Region to test: "
                                        + testRect
                                        + "\nfirst empty point: "
                                        + x
                                        + ", "
                                        + y;
                        reason += "\nvisible regions:";
                        for (Layer layer : layers) {
                            if (layer.isInvisible() || layer.isHiddenByParent()) {
                                continue;
                            }
                            Rect r = extract(layer.mProto.visibleRegion);
                            reason += "\n" + layer.mProto.name + r.toString();
                        }
                        return new Result(
                                false /* success */, this.mTimestamp, assertionName, reason);
                    }
                }
            }
            String info = "Region covered: " + testRect;
            return new Result(true /* success */, this.mTimestamp, assertionName, info);
        }

        /**
         * Checks if a layer with name {@code layerName} has a visible region {@code
         * expectedVisibleRegion}.
         */
        public Result hasVisibleRegion(String layerName, Rect expectedVisibleRegion) {
            String assertionName = "hasVisibleRegion";
            String reason = "Could not find " + layerName;
            for (Layer layer : asFlattenedLayers()) {
                if (layer.mProto.name.contains(layerName)) {
                    if (layer.isHiddenByParent()) {
                        reason = layer.getHiddenByParentReason();
                        continue;
                    }
                    if (layer.isInvisible()) {
                        reason = layer.getVisibilityReason();
                        continue;
                    }
                    Rect visibleRegion = extract(layer.mProto.visibleRegion);
                    if (visibleRegion.equals(expectedVisibleRegion)) {
                        return new Result(
                                true /* success */,
                                this.mTimestamp,
                                assertionName,
                                layer.mProto.name + "has visible region " + expectedVisibleRegion);
                    }
                    reason =
                            layer.mProto.name
                                    + " has visible region:"
                                    + visibleRegion
                                    + " "
                                    + "expected:"
                                    + expectedVisibleRegion;
                }
            }
            return new Result(false /* success */, this.mTimestamp, assertionName, reason);
        }

        /** Checks if a layer with name {@code layerName} is visible. */
        public Result isVisible(String layerName) {
            String assertionName = "isVisible";
            String reason = "Could not find " + layerName;
            for (Layer layer : asFlattenedLayers()) {
                if (layer.mProto.name.contains(layerName)) {
                    if (layer.isHiddenByParent()) {
                        reason = layer.getHiddenByParentReason();
                        continue;
                    }
                    if (layer.isInvisible()) {
                        reason = layer.getVisibilityReason();
                        continue;
                    }
                    return new Result(
                            true /* success */,
                            this.mTimestamp,
                            assertionName,
                            layer.mProto.name + " is visible");
                }
            }
            return new Result(false /* success */, this.mTimestamp, assertionName, reason);
        }

        @Override
        public long getTimestamp() {
            return mTimestamp;
        }

        public List<Layer> getRootLayers() {
            return mRootLayers;
        }

        /** Returns all layers as a flattened list using a depth first traversal. */
        public List<Layer> asFlattenedLayers() {
            if (mFlattenedLayers == null) {
                mFlattenedLayers = new ArrayList<>();
                ArrayList<Layer> pendingLayers = new ArrayList<>(this.mRootLayers);
                while (!pendingLayers.isEmpty()) {
                    Layer layer = pendingLayers.remove(0);
                    mFlattenedLayers.add(layer);
                    pendingLayers.addAll(layer.mChildren);
                }
            }
            return mFlattenedLayers;
        }

        public Rect getVisibleBounds(String layerName) {
            List<Layer> layers = asFlattenedLayers();
            for (Layer layer : layers) {
                if (layer.mProto.name.contains(layerName) && layer.isVisible()) {
                    return extract(layer.mProto.visibleRegion);
                }
            }
            return new Rect(0, 0, 0, 0);
        }
    }

    /** Represents a single layer with links to its parent and child layers. */
    public static class Layer {
        @Nullable public LayerProto mProto;
        public List<Layer> mChildren;
        @Nullable public Layer mParent = null;

        private Layer(LayerProto proto) {
            this.mProto = proto;
            this.mChildren = new ArrayList<>();
        }

        private void addChild(Layer childLayer) {
            this.mChildren.add(childLayer);
        }

        private void addParent(Layer parentLayer) {
            this.mParent = parentLayer;
        }

        public int getId() {
            return mProto.id;
        }

        public int getParentId() {
            return mProto.parent;
        }

        public String getName() {
            if (mProto != null) {
                return mProto.name;
            }

            return "";
        }

        public boolean isActiveBufferEmpty() {
            return this.mProto.activeBuffer == null
                    || this.mProto.activeBuffer.height == 0
                    || this.mProto.activeBuffer.width == 0;
        }

        public boolean isVisibleRegionEmpty() {
            if (this.mProto.visibleRegion == null) {
                return true;
            }
            Rect visibleRect = Entry.extract(this.mProto.visibleRegion);
            return visibleRect.height() == 0 || visibleRect.width() == 0;
        }

        public boolean isHidden() {
            return (this.mProto.flags & /* FLAG_HIDDEN */ 0x1) != 0x0;
        }

        public boolean isVisible() {
            return (!isActiveBufferEmpty() || isColorLayer())
                    && !isHidden()
                    && this.mProto.color != null
                    && this.mProto.color.a > 0
                    && !isVisibleRegionEmpty();
        }

        public boolean isColorLayer() {
            return this.mProto.type.equals("ColorLayer");
        }

        public boolean isRootLayer() {
            return mParent != null && mParent.mProto == null;
        }

        public boolean isInvisible() {
            return !isVisible();
        }

        public boolean isHiddenByParent() {
            return !isRootLayer() && (mParent.isHidden() || mParent.isHiddenByParent());
        }

        public String getHiddenByParentReason() {
            String reason = "Layer " + mProto.name;
            if (isHiddenByParent()) {
                reason += " is hidden by parent: " + mParent.mProto.name;
            } else {
                reason += " is not hidden by parent: " + mParent.mProto.name;
            }
            return reason;
        }

        public String getVisibilityReason() {
            String reason = "Layer " + mProto.name;
            if (isVisible()) {
                reason += " is visible:";
            } else {
                reason += " is invisible:";
                if (this.mProto.activeBuffer == null) {
                    reason += " activeBuffer=null";
                } else if (this.mProto.activeBuffer.height == 0) {
                    reason += " activeBuffer.height=0";
                } else if (this.mProto.activeBuffer.width == 0) {
                    reason += " activeBuffer.width=0";
                }
                if (!isColorLayer()) {
                    reason += " type != ColorLayer";
                }
                if (isHidden()) {
                    reason += " flags=" + this.mProto.flags + " (FLAG_HIDDEN set)";
                }
                if (this.mProto.color == null || this.mProto.color.a == 0) {
                    reason += " color.a=0";
                }
                if (isVisibleRegionEmpty()) {
                    reason += " visible region is empty";
                }
            }
            return reason;
        }
    }
}
