// Copyright TBD
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#ifndef PACKAGER_LIVE_PACKAGER_H_
#define PACKAGER_LIVE_PACKAGER_H_

#include <memory>
#include <string>

#include <absl/synchronization/notification.h>

#include <packager/file/file.h>
#include <packager/file/io_cache.h>

namespace shaka {

/// TODO: LivePackager handles packaging fragments

class Segment {
public:
  Segment() = default;
  ~Segment() = default;

  Segment(const uint8_t *data, size_t size); 
  explicit Segment(const char *fname); 

  const uint8_t *data() const;
  size_t size() const;
  
  uint64_t SequenceNumber() const;
  void SetSequenceNumber(uint64_t n);

private:
  std::vector<uint8_t> data_;
};

struct LiveConfig {
  enum class StreamingProtocol {
    DASH,
    HLS,
  };

  enum class TrackType {
    AUDIO,
    VIDEO,
  };

  StreamingProtocol protocol;
  TrackType track_type;
  // TOOD: do we need non-integer durations?
  double segment_duration_in_seconds;
};

class LivePackager {
public:
  LivePackager(const LiveConfig &config);
  ~LivePackager();

  Status Package(const Segment &init, const Segment &segment);
  const Segment &GetInitSegment() const;
  const Segment &GetSegment() const;

  LivePackager(const LivePackager&) = delete;
  LivePackager& operator=(const LivePackager&) = delete;

private:
  uint64_t segment_count_ = 0u;
  shaka::Segment init_segment_;
  shaka::Segment segment_;
  LiveConfig config_;
};

}  // namespace shaka

#endif  // PACKAGER_LIVE_PACKAGER_H_
