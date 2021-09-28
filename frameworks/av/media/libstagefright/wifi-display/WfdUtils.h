#ifndef WFD_UTILS_H_

#define WFD_UTILS_H_

#include <media/MediaCodecBuffer.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <utils/Errors.h>

namespace android {

bool IsIDR(const sp<ABuffer> &accessUnit);
bool IsIDR(const sp<MediaCodecBuffer> &accessUnit);

}  // namespace android

#endif  // WFD_UTILS_H_
