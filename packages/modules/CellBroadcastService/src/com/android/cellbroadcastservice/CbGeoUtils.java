/*
 * Copyright (C) 2019 The Android Open Source Project
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

package com.android.cellbroadcastservice;

import android.annotation.NonNull;
import android.telephony.CbGeoUtils.Circle;
import android.telephony.CbGeoUtils.Geometry;
import android.telephony.CbGeoUtils.LatLng;
import android.telephony.CbGeoUtils.Polygon;
import android.text.TextUtils;
import android.util.Log;

import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.Objects;
import java.util.Optional;
import java.util.stream.Collectors;

/**
 * This utils class is specifically used for geo-targeting of CellBroadcast messages.
 * The coordinates used by this utils class are latitude and longitude, but some algorithms in this
 * class only use them as coordinates on plane, so the calculation will be inaccurate. So don't use
 * this class for anything other then geo-targeting of cellbroadcast messages.
 */
public class CbGeoUtils {
    /**
     * Tolerance for determining if the value is 0. If the absolute value of a value is less than
     * this tolerance, it will be treated as 0.
     */
    public static final double EPS = 1e-7;

    private static final String TAG = "CbGeoUtils";

    /** The TLV tags of WAC, defined in ATIS-0700041 5.2.3 WAC tag coding. */
    public static final int GEO_FENCING_MAXIMUM_WAIT_TIME = 0x01;
    public static final int GEOMETRY_TYPE_POLYGON = 0x02;
    public static final int GEOMETRY_TYPE_CIRCLE = 0x03;

    /** The identifier of geometry in the encoded string. */
    private static final String CIRCLE_SYMBOL = "circle";
    private static final String POLYGON_SYMBOL = "polygon";

    /**
     * Parse the geometries from the encoded string {@code str}. The string must follow the
     * geometry encoding specified by {@link android.provider.Telephony.CellBroadcasts#GEOMETRIES}.
     */
    @NonNull
    public static List<Geometry> parseGeometriesFromString(@NonNull String str) {
        List<Geometry> geometries = new ArrayList<>();
        for (String geometryStr : str.split("\\s*;\\s*")) {
            String[] geoParameters = geometryStr.split("\\s*\\|\\s*");
            switch (geoParameters[0]) {
                case CIRCLE_SYMBOL:
                    geometries.add(new Circle(parseLatLngFromString(geoParameters[1]),
                            Double.parseDouble(geoParameters[2])));
                    break;
                case POLYGON_SYMBOL:
                    List<LatLng> vertices = new ArrayList<>(geoParameters.length - 1);
                    for (int i = 1; i < geoParameters.length; i++) {
                        vertices.add(parseLatLngFromString(geoParameters[i]));
                    }
                    geometries.add(new Polygon(vertices));
                    break;
                default:
                    final String errorMessage = "Invalid geometry format " + geometryStr;
                    Log.e(TAG, errorMessage);
                    CellBroadcastStatsLog.write(CellBroadcastStatsLog.CB_MESSAGE_ERROR,
                            CellBroadcastStatsLog.CELL_BROADCAST_MESSAGE_ERROR__TYPE__UNEXPECTED_GEOMETRY_FROM_FWK,
                            errorMessage);
            }
        }
        return geometries;
    }

    /**
     * Encode a list of geometry objects to string. The encoding format is specified by
     * {@link android.provider.Telephony.CellBroadcasts#GEOMETRIES}.
     *
     * @param geometries the list of geometry objects need to be encoded.
     * @return the encoded string.
     */
    @NonNull
    public static String encodeGeometriesToString(@NonNull List<Geometry> geometries) {
        return geometries.stream()
                .map(geometry -> encodeGeometryToString(geometry))
                .filter(encodedStr -> !TextUtils.isEmpty(encodedStr))
                .collect(Collectors.joining(";"));
    }


    /**
     * Encode the geometry object to string. The encoding format is specified by
     * {@link android.provider.Telephony.CellBroadcasts#GEOMETRIES}.
     * @param geometry the geometry object need to be encoded.
     * @return the encoded string.
     */
    @NonNull
    private static String encodeGeometryToString(@NonNull Geometry geometry) {
        StringBuilder sb = new StringBuilder();
        if (geometry instanceof Polygon) {
            sb.append(POLYGON_SYMBOL);
            for (LatLng latLng : ((Polygon) geometry).getVertices()) {
                sb.append("|");
                sb.append(latLng.lat);
                sb.append(",");
                sb.append(latLng.lng);
            }
        } else if (geometry instanceof Circle) {
            sb.append(CIRCLE_SYMBOL);
            Circle circle = (Circle) geometry;

            // Center
            sb.append("|");
            sb.append(circle.getCenter().lat);
            sb.append(",");
            sb.append(circle.getCenter().lng);

            // Radius
            sb.append("|");
            sb.append(circle.getRadius());
        } else {
            Log.e(TAG, "Unsupported geometry object " + geometry);
            return null;
        }
        return sb.toString();
    }

    /**
     * Parse {@link LatLng} from {@link String}. Latitude and longitude are separated by ",".
     * Example: "13.56,-55.447".
     *
     * @param str encoded lat/lng string.
     * @Return {@link LatLng} object.
     */
    @NonNull
    private static LatLng parseLatLngFromString(@NonNull String str) {
        String[] latLng = str.split("\\s*,\\s*");
        return new LatLng(Double.parseDouble(latLng[0]), Double.parseDouble(latLng[1]));
    }

    private static final double SCALE = 1000.0 * 100.0;


    /**
     * Computes the shortest distance of {@code geo} to {@code latLng}.  If {@code geo} does not
     * support this functionality, {@code Optional.empty()} is returned.
     *
     * @hide
     * @param geo shape
     * @param latLng point to calculate against
     * @return the distance in meters
     */
    @VisibleForTesting
    public static Optional<Double> distance(Geometry geo,
            @NonNull LatLng latLng) {
        if (geo instanceof android.telephony.CbGeoUtils.Polygon) {
            CbGeoUtils.DistancePolygon distancePolygon =
                    new CbGeoUtils.DistancePolygon((Polygon) geo);
            return Optional.of(distancePolygon.distance(latLng));
        } else if (geo instanceof android.telephony.CbGeoUtils.Circle) {
            CbGeoUtils.DistanceCircle distanceCircle =
                    new CbGeoUtils.DistanceCircle((Circle) geo);
            return Optional.of(distanceCircle.distance(latLng));
        } else {
            return Optional.empty();
        }
    }

    /**
     * Will be merged with {@code CbGeoUtils.Circle} in future release.
     *
     * @hide
     */
    @VisibleForTesting
    public static class DistanceCircle {
        private final Circle mCircle;

        DistanceCircle(Circle circle) {
            mCircle = circle;
        }

        /**
         * Distance in meters.  If you are within the bounds of the circle, returns a
         * negative distance to the edge.
         * @param latLng the coordinate to calculate distance against
         * @return the distance given in meters
         */
        public double distance(@NonNull final LatLng latLng) {
            return latLng.distance(mCircle.getCenter()) - mCircle.getRadius();
        }
    }
    /**
     * Will be merged with {@code CbGeoUtils.Polygon} in future release.
     *
     * @hide
     */
    @VisibleForTesting
    public static class DistancePolygon {

        @NonNull private final Polygon mPolygon;
        @NonNull private final LatLng mOrigin;

        public DistancePolygon(@NonNull final Polygon polygon) {
            mPolygon = polygon;

            // Find the point with smallest longitude as the mOrigin point.
            int idx = 0;
            for (int i = 1; i < polygon.getVertices().size(); i++) {
                if (polygon.getVertices().get(i).lng < polygon.getVertices().get(idx).lng) {
                    idx = i;
                }
            }
            mOrigin = polygon.getVertices().get(idx);
        }

        /**
         * Returns the meters difference between {@code latLng} to the closest point in the polygon.
         *
         * Note: The distance given becomes less accurate as you move further north and south.
         *
         * @param latLng the coordinate to calculate distance against
         * @return the distance given in meters
         */
        public double distance(@NonNull final LatLng latLng) {
            double minDistance = Double.MAX_VALUE;
            List<LatLng> vertices = mPolygon.getVertices();
            int n = mPolygon.getVertices().size();
            for (int i = 0; i < n; i++) {
                LatLng a = vertices.get(i);
                LatLng b = vertices.get((i + 1) % n);

                // The converted points are distances (in meters) to the origin point.
                // see: #convertToDistanceFromOrigin
                Point sa = convertToDistanceFromOrigin(a);
                Point sb = convertToDistanceFromOrigin(b);
                Point sp = convertToDistanceFromOrigin(latLng);

                CbGeoUtils.LineSegment l = new CbGeoUtils.LineSegment(sa, sb);
                double d = l.distance(sp);
                minDistance = Math.min(d, minDistance);
            }
            return minDistance;
        }

        /**
         * Move the given point {@code latLng} to the coordinate system with {@code mOrigin} as the
         * origin. {@code mOrigin} is selected from the vertices of a polygon, it has
         * the smallest longitude value among all of the polygon vertices.  The unit distance
         * between points is meters.
         *
         * @param latLng the point need to be converted and scaled.
         * @Return a {@link Point} object
         */
        private Point convertToDistanceFromOrigin(LatLng latLng) {
            return CbGeoUtils.convertToDistanceFromOrigin(mOrigin, latLng);
        }
    }

    /**
     * We calculate the new point by finding the distances between the latitude and longitude
     * components independently from {@code latLng} to the {@code origin}.
     *
     * This ends up giving us a {@code Point} such that:
     * {@code x = distance(latLng.lat, origin.lat)}
     * {@code y = distance(latLng.lng, origin.lng)}
     *
     * This allows us to use simple formulas designed for a cartesian coordinate system when
     * calculating the distance from a point to a line segment.
     *
     * @param origin the origin lat lng in which to convert and scale {@code latLng}
     * @param latLng the lat lng need to be converted and scaled.
     * @return a {@link Point} object.
     *
     * @hide
     */
    @VisibleForTesting
    public static Point convertToDistanceFromOrigin(@NonNull final LatLng origin,
            @NonNull final LatLng latLng) {
        double x = new LatLng(latLng.lat, origin.lng).distance(new LatLng(origin.lat, origin.lng));
        double y = new LatLng(origin.lat, latLng.lng).distance(new LatLng(origin.lat, origin.lng));

        x = latLng.lat > origin.lat ? x : -x;
        y = latLng.lng > origin.lng ? y : -y;
        return new Point(x, y);
    }

    /**
     * @hide
     */
    @VisibleForTesting
    public static class Point {
        /**
         * x-coordinate
         */
        public final double x;

        /**
         * y-coordinate
         */
        public final double y;

        /**
         * ..ctor
         * @param x
         * @param y
         */
        public Point(double x, double y) {
            this.x = x;
            this.y = y;
        }

        /**
         * Subtracts the two points
         * @param p
         * @return
         */
        public Point subtract(Point p) {
            return new Point(x - p.x, y - p.y);
        }

        /**
         * Calculates the distance between the two points
         * @param pt
         * @return
         */
        public double distance(Point pt) {
            return Math.sqrt(Math.pow(x - pt.x, 2) + Math.pow(y - pt.y, 2));
        }

        @Override
        public boolean equals(Object o) {
            if (this == o) return true;
            if (o == null || getClass() != o.getClass()) return false;
            Point point = (Point) o;
            return Double.compare(point.x, x) == 0
                    && Double.compare(point.y, y) == 0;
        }

        @Override
        public int hashCode() {
            return Objects.hash(x, y);
        }

        @Override
        public String toString() {
            return "(" + x + ", " + y + ")";
        }
    }

    /**
     * Represents a line segment. This is used for handling geo-fenced cell broadcasts.
     * More information regarding cell broadcast geo-fencing logic is
     * laid out in 3GPP TS 23.041 and ATIS-0700041.
     *
     * @hide
     */
    @VisibleForTesting
    public static final class LineSegment {
        @NonNull final Point a, b;

        public LineSegment(@NonNull final Point a, @NonNull final Point b) {
            this.a = a;
            this.b = b;
        }

        public double getLength() {
            return this.a.distance(this.b);
        }

        /**
         * Calculates the closest distance from {@code pt} to this line segment.
         *
         * @param pt the point to calculate against
         * @return the distance in meters
         */
        public double distance(Point pt) {
            final double lengthSquared = getLength() * getLength();
            if (lengthSquared == 0.0) {
                return pt.distance(this.a);
            }

            Point sub1 = pt.subtract(a);
            Point sub2 = b.subtract(a);
            double dot = sub1.x * sub2.x + sub1.y * sub2.y;

            //Magnitude of projection
            double magnitude = dot / lengthSquared;

            //Keep bounded between 0.0 and 1.0
            if (magnitude > 1.0) {
                magnitude = 1.0;
            } else if (magnitude < 0.0) {
                magnitude = 0.0;
            }

            final double projX = calcProjCoordinate(this.a.x, this.b.x, magnitude);
            final double projY = calcProjCoordinate(this.a.y, this.b.y, magnitude);

            final Point proj = new Point(projX, projY);
            return proj.distance(pt);
        }

        private static double calcProjCoordinate(double aVal, double bVal, double m) {
            return aVal + ((bVal - aVal) * m);
        }
    }
}
