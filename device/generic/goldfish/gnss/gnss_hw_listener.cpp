/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <chrono>
#include <log/log.h>
#include <utils/SystemClock.h>
#include "gnss_hw_listener.h"
#include "util.h"

namespace goldfish {
namespace {
const char* testNmeaField(const char* i, const char* end,
                          const char* v,
                          const char sep) {
    while (i < end) {
        if (*v == 0) {
            return (*i == sep) ? (i + 1) : nullptr;
        } else if (*v == *i) {
            ++v;
            ++i;
        } else {
            return nullptr;
        }
    }

    return nullptr;
}

const char* skipAfter(const char* i, const char* end, const char c) {
    for (; i < end; ++i) {
        if (*i == c) {
            return i + 1;
        }
    }
    return nullptr;
}

double convertDMMF(const int dmm, const int f, int p10) {
    const int d = dmm / 100;
    const int m = dmm % 100;
    int base10 = 1;
    for (; p10 > 0; --p10) { base10 *= 10; }

    return double(d) + (m + (f / double(base10))) / 60.0;
}

double sign(char m, char positive) { return (m == positive) ? 1.0 : -1; }

}  // namespace

GnssHwListener::GnssHwListener(const DataSink* sink): m_sink(sink) {
    m_buffer.reserve(256);
}

void GnssHwListener::reset() {
    m_buffer.clear();
}

void GnssHwListener::consume(char c) {
    if (c == '$' || !m_buffer.empty()) {
        m_buffer.push_back(c);
    }
    if (c == '\n') {
        const ahg20::ElapsedRealtime ts = util::makeElapsedRealtime(util::nowNanos());

        if (parse(m_buffer.data() + 1, m_buffer.data() + m_buffer.size() - 2, ts)) {
            m_sink->gnssNmea(ts.timestampNs / 1000000,
                             hidl_string(m_buffer.data(), m_buffer.size()));
        } else {
            m_buffer.back() = 0;
            ALOGW("%s:%d: failed to parse an NMEA message, '%s'",
                  __PRETTY_FUNCTION__, __LINE__, m_buffer.data());
        }
        m_buffer.clear();
    } else if (m_buffer.size() >= 1024) {
        ALOGW("%s:%d buffer was too long, dropped", __PRETTY_FUNCTION__, __LINE__);
        m_buffer.clear();
    }
}

bool GnssHwListener::parse(const char* begin, const char* end,
                           const ahg20::ElapsedRealtime& ts) {
    if (const char* fields = testNmeaField(begin, end, "GPRMC", ',')) {
        return parseGPRMC(fields, end, ts);
    } else if (const char* fields = testNmeaField(begin, end, "GPGGA", ',')) {
        return parseGPGGA(fields, end, ts);
    } else {
        return false;
    }
}

//        begin                                                          end
// $GPRMC,195206,A,1000.0000,N,10000.0000,E,173.8,231.8,010420,004.2,W*47
//          1    2    3      4    5       6     7     8      9    10 11 12
//      1  195206     Time Stamp
//      2  A          validity - A-ok, V-invalid
//      3  1000.0000  current Latitude
//      4  N          North/South
//      5  10000.0000 current Longitude
//      6  E          East/West
//      7  173.8      Speed in knots
//      8  231.8      True course
//      9  010420     Date Stamp (13 June 1994)
//     10  004.2      Variation
//     11  W          East/West
//     12  *70        checksum
bool GnssHwListener::parseGPRMC(const char* begin, const char*,
                                const ahg20::ElapsedRealtime& ts) {
    double speedKnots = 0;
    double course = 0;
    double variation = 0;
    int latdmm = 0;
    int londmm = 0;
    int latf = 0;
    int lonf = 0;
    int latdmmConsumed = 0;
    int latfConsumed = 0;
    int londmmConsumed = 0;
    int lonfConsumed = 0;
    int hhmmss = -1;
    int ddmoyy = 0;
    char validity = 0;
    char ns = 0;    // north/south
    char ew = 0;    // east/west
    char var_ew = 0;

    if (sscanf(begin, "%06d,%c,%d.%n%d%n,%c,%d.%n%d%n,%c,%lf,%lf,%d,%lf,%c*",
               &hhmmss, &validity,
               &latdmm, &latdmmConsumed, &latf, &latfConsumed, &ns,
               &londmm, &londmmConsumed, &lonf, &lonfConsumed, &ew,
               &speedKnots, &course,
               &ddmoyy,
               &variation, &var_ew) != 13) {
        return false;
    }
    if (validity != 'A') {
        return false;
    }

    const double lat = convertDMMF(latdmm, latf, latfConsumed - latdmmConsumed) * sign(ns, 'N');
    const double lon = convertDMMF(londmm, lonf, lonfConsumed - londmmConsumed) * sign(ew, 'E');
    const double speed = speedKnots * 0.514444;

    ahg20::GnssLocation loc20;
    loc20.elapsedRealtime = ts;

    auto& loc10 = loc20.v1_0;

    loc10.latitudeDegrees = lat;
    loc10.longitudeDegrees = lon;
    loc10.speedMetersPerSec = speed;
    loc10.bearingDegrees = course;
    loc10.horizontalAccuracyMeters = 5;
    loc10.speedAccuracyMetersPerSecond = .5;
    loc10.bearingAccuracyDegrees = 30;
    loc10.timestamp = ts.timestampNs / 1000000;

    using ahg10::GnssLocationFlags;
    loc10.gnssLocationFlags =
        GnssLocationFlags::HAS_LAT_LONG |
        GnssLocationFlags::HAS_SPEED |
        GnssLocationFlags::HAS_BEARING |
        GnssLocationFlags::HAS_HORIZONTAL_ACCURACY |
        GnssLocationFlags::HAS_SPEED_ACCURACY |
        GnssLocationFlags::HAS_BEARING_ACCURACY;

    if (m_flags & GnssLocationFlags::HAS_ALTITUDE) {
        loc10.altitudeMeters = m_altitude;
        loc10.verticalAccuracyMeters = .5;
        loc10.gnssLocationFlags |= GnssLocationFlags::HAS_ALTITUDE |
                                   GnssLocationFlags::HAS_VERTICAL_ACCURACY;

    }

    m_sink->gnssLocation(loc20);
    return true;
}

// $GPGGA,123519,4807.0382,N,12204.9799,W,1,6,,4.2,M,0.,M,,,*47
//    time of fix      123519     12:35:19 UTC
//    latitude         4807.0382  48 degrees, 07.0382 minutes
//    north/south      N or S
//    longitude        12204.9799 122 degrees, 04.9799 minutes
//    east/west        E or W
//    fix quality      1          standard GPS fix
//    satellites       1 to 12    number of satellites being tracked
//    HDOP             <dontcare> horizontal dilution
//    altitude         4.2        altitude above sea-level
//    altitude units   M          to indicate meters
//    diff             <dontcare> height of sea-level above ellipsoid
//    diff units       M          to indicate meters (should be <dontcare>)
//    dgps age         <dontcare> time in seconds since last DGPS fix
//    dgps sid         <dontcare> DGPS station id
bool GnssHwListener::parseGPGGA(const char* begin, const char* end,
                                const ahg20::ElapsedRealtime&) {
    double altitude = 0;
    int latdmm = 0;
    int londmm = 0;
    int latf = 0;
    int lonf = 0;
    int latdmmConsumed = 0;
    int latfConsumed = 0;
    int londmmConsumed = 0;
    int lonfConsumed = 0;
    int hhmmss = 0;
    int fixQuality = 0;
    int nSatellites = 0;
    int consumed = 0;
    char ns = 0;
    char ew = 0;
    char altitudeUnit = 0;

    if (sscanf(begin, "%06d,%d.%n%d%n,%c,%d.%n%d%n,%c,%d,%d,%n",
               &hhmmss,
               &latdmm, &latdmmConsumed, &latf, &latfConsumed, &ns,
               &londmm, &londmmConsumed, &lonf, &lonfConsumed, &ew,
               &fixQuality,
               &nSatellites,
               &consumed) != 9) {
        return false;
    }

    begin = skipAfter(begin + consumed, end, ',');  // skip HDOP
    if (!begin) {
        return false;
    }
    if (sscanf(begin, "%lf,%c,", &altitude, &altitudeUnit) != 2) {
        return false;
    }
    if (altitudeUnit != 'M') {
        return false;
    }

    m_altitude = altitude;
    m_flags |= ahg10::GnssLocationFlags::HAS_ALTITUDE;

    hidl_vec<ahg20::IGnssCallback::GnssSvInfo> svInfo(nSatellites);
    for (int i = 0; i < nSatellites; ++i) {
        auto* info20 = &svInfo[i];
        auto* info10 = &info20->v1_0;

        info20->constellation = ahg20::GnssConstellationType::GPS;
        info10->svid = i + 3;
        info10->constellation = ahg10::GnssConstellationType::GPS;
        info10->cN0Dbhz = 30;
        info10->elevationDegrees = 0;
        info10->azimuthDegrees = 0;
        info10->carrierFrequencyHz = 1.59975e+09;
        info10->svFlag = ahg10::IGnssCallback::GnssSvFlags::HAS_CARRIER_FREQUENCY | 0;
    }

    m_sink->gnssSvStatus(svInfo);

    return true;
}

}  // namespace goldfish
