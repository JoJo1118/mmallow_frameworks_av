/*
 * Copyright (C) 2009 The Android Open Source Project
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

#define LOG_NDEBUG 0
#define LOG_TAG "MediaExtractor"
#include <utils/Log.h>

#include "include/AMRExtractor.h"
#include "include/MP3Extractor.h"
#include "include/MPEG4Extractor.h"
#include "include/WAVExtractor.h"
#include "include/OggExtractor.h"
#include "include/MPEG2PSExtractor.h"
#include "include/MPEG2TSExtractor.h"
#include "include/DRMExtractor.h"
#include "include/WVMExtractor.h"
#include "include/FLACExtractor.h"
#include "include/AACExtractor.h"
#include "include/MidiExtractor.h"

#include "matroska/MatroskaExtractor.h"

#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/DataSource.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/MediaExtractor.h>
#include <media/stagefright/MetaData.h>
#include <utils/String8.h>

#ifdef WITH_AMLOGIC_MEDIA_EX_SUPPORT
#include <media/amlogic/amExtratorSupport.h>
#include <media/stagefright/AmMediaDefsExt.h>
#endif

namespace android {

sp<MetaData> MediaExtractor::getMetaData() {
    return new MetaData;
}

uint32_t MediaExtractor::flags() const {
    return CAN_SEEK_BACKWARD | CAN_SEEK_FORWARD | CAN_PAUSE | CAN_SEEK;
}


// static
sp<MediaExtractor> MediaExtractor::Create(
    const sp<DataSource> &source, const char *mime)
{
    sp<AMessage> meta(NULL);
    sp<MediaExtractor> extractor;
    String8 tmp("");
    int is_sniff_from_ffmpeg = 0;
    if (mime == NULL) {
        float confidence = 0;

        if (!source->sniff(&tmp, &confidence, &meta)) {
            confidence = 0;
        }
#ifdef WITH_AMLOGIC_MEDIA_EX_SUPPORT
        if (confidence < 0.8 ||
        (!strcmp(tmp.string(), MEDIA_MIMETYPE_AUDIO_WMA)))
        {
            float ffconfidence = 0;
            String8 tmpffmpeg("");
            sp<AMessage> ffmeta(NULL);
            if (!sniffFFmpegFormat(source, &tmpffmpeg, &ffconfidence, &ffmeta) && confidence <= 0.0)
            {
                ALOGE("FAILED to autodetect media content.");
                return NULL;
            } else {
                if (confidence == 0 ||
                   ffconfidence > confidence ||
                   (ffconfidence > 0 && strcmp(tmpffmpeg.string(), tmp.string()))) {
                    is_sniff_from_ffmpeg = 1;
                    confidence = ffconfidence;
                    tmp = tmpffmpeg;
                }
            }
        }
#else
    if (confidence == 0) {
        ALOGE("FAILED to autodetect media content from datasource.");
        return NULL;
    }
#endif


        mime = tmp.string();
        ALOGE("Autodetected media content as '%s' with confidence %.2f from_ffmpeg=%d",
              mime, confidence, is_sniff_from_ffmpeg);
    }

    bool isDrm = false;
    // DRM MIME type syntax is "drm+type+original" where
    // type is "es_based" or "container_based" and
    // original is the content's cleartext MIME type
    if (!strncmp(mime, "drm+", 4)) {
        const char *originalMime = strchr(mime + 4, '+');
        if (originalMime == NULL) {
            // second + not found
            return NULL;
        }
        ++originalMime;
        if (!strncmp(mime, "drm+es_based+", 13)) {
            // DRMExtractor sets container metadata kKeyIsDRM to 1
            return new DRMExtractor(source, originalMime);
        } else if (!strncmp(mime, "drm+container_based+", 20)) {
            mime = originalMime;
            isDrm = true;
        } else {
            return NULL;
        }
    }

    MediaExtractor *ret = NULL;
#ifdef WITH_AMLOGIC_MEDIA_EX_SUPPORT
    if (is_sniff_from_ffmpeg) {
        extractor = createFFmpegExtractor(source, mime);
        if (extractor != NULL) {
            ret = extractor.get();
        }
    }
#endif
    if (ret != NULL) {
        /**/
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG4)
               || !strcasecmp(mime, "audio/mp4")) {
        ret = new MPEG4Extractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_MPEG)) {
        ret = new MP3Extractor(source, meta);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AMR_NB)
               || !strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AMR_WB)) {
        ret = new AMRExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_FLAC)) {
        ret = new FLACExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_WAV)) {
        ret = new WAVExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_OGG)) {
        ret = new OggExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MATROSKA)) {
        ret = new MatroskaExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG2TS)) {
        ret = new MPEG2TSExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_WVM)) {
        // Return now.  WVExtractor should not have the DrmFlag set in the block below.
        return new WVMExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_AAC_ADTS)) {
        ret = new AACExtractor(source, meta);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_CONTAINER_MPEG2PS)) {
        ret = new MPEG2PSExtractor(source);
    } else if (!strcasecmp(mime, MEDIA_MIMETYPE_AUDIO_MIDI)) {
        ret = new MidiExtractor(source);
    }
#ifdef WITH_AMLOGIC_MEDIA_EX_SUPPORT
    if (ret == NULL) {
        extractor = createAmExExtractor(source, mime, meta);
        if (extractor != NULL) {
            ret = extractor.get();
        }
    }
#endif
    if (ret != NULL) {
        if (isDrm) {
            ret->setDrmFlag(true);
        } else {
            ret->setDrmFlag(false);
        }
    }
    ALOGE("return createAmExExtractor. ret %x\n", ret);
    return ret;
}

}  // namespace android
