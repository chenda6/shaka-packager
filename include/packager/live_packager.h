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

#include <packager/packager.h>

namespace shaka {

/// TODO: LivePackager handles packaging fragments

class Segment {
public:
  Segment() = default;
  ~Segment() = default;

  Segment(const uint8_t *data, size_t size); 
  Segment(const std::vector<uint8_t> &data);

  const uint8_t *data() const;
  uint8_t *data();
  size_t size() const;

  void Insert(const uint8_t* data, size_t size);
  
private:
  const uint8_t *data_ = nullptr;
  size_t size_ = 0;
  std::vector<uint8_t> buffer_;
};

struct FullSegment {
public:
  FullSegment() = default;
  ~FullSegment() = default;

  void SetInitSegment(const uint8_t *data, size_t size);
  void AppendData(const uint8_t *data, size_t size);

  const std::vector<uint8_t> & GetBuffer() const;

  size_t GetInitSegmentSize() const;
  size_t GetSegmentSize() const;

private:
  /// buffer is expected to contain both the init and data segments, i.e.,
  // (ftyp + moov) + (moof + mdat)
  std::vector<uint8_t> buffer_;
  /// @brief  Indicates the how much the init segment occupies data_
  size_t init_segment_size_ = 0;
};

struct LiveConfig {
  enum class OutputFormat {
    FMP4,
    TS,
  };

  enum class TrackType {
    AUDIO,
    VIDEO,
  };

  OutputFormat format;
  TrackType track_type;
  // TOOD: do we need non-integer durations?
  double segment_duration_sec;
};

class LivePackager {
public:
  LivePackager(const LiveConfig &config);
  ~LivePackager();

  Status Package(const FullSegment &input, FullSegment &output);

  LivePackager(const LivePackager&) = delete;
  LivePackager& operator=(const LivePackager&) = delete;

private:
  uint64_t segment_count_ = 0u;
  LiveConfig config_;
};

}  // namespace shaka

#endif  // PACKAGER_LIVE_PACKAGER_H_
