// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <fstream>
#include <sstream>

#include <packager/packager.h>
#include <packager/file/file.h>
#include <packager/file/file_closer.h>
#include <packager/live_packager.h>
#include "live_packager.h"

namespace shaka {

namespace {

using StreamDescriptors = std::vector<shaka::StreamDescriptor>;

const std::string DEFAULT_INPUT_FNAME = "memory://input_file";
const std::string DEFAULT_INIT_SEGMENT_FNAME = "init.mp4";

std::string getSegmentTemplate(const LiveConfig &config) {
  return config.protocol == LiveConfig::StreamingProtocol::HLS ? "$Number$.ts" : "$Number$.m4s";
}

std::string getStreamSelector(const LiveConfig &config) {
  return config.track_type == LiveConfig::TrackType::VIDEO ? "video" : "audio";
}

}

#define TAG(a, b, c, d)                                               \
  ((static_cast<uint32_t>(static_cast<uint8_t>(a)) << 24) |           \
   (static_cast<uint8_t>(b) << 16) | (static_cast<uint8_t>(c) << 8) | \
   (static_cast<uint8_t>(d)))

Segment::Segment(const uint8_t *data, size_t size) 
    : data_(data, data + size) {}

Segment::Segment(const char *fname) {
  std::ifstream fin(fname, std::ios::binary);

  fin.seekg(0, std::ios::end);
  data_.resize(fin.tellg());
  fin.seekg(0, std::ios::beg);

  fin.read(reinterpret_cast<char*>(data_.data()), data_.size());
}

const uint8_t *Segment::data() const {
    return data_.data();
}

size_t Segment::size() const {
    return data_.size();
}

uint64_t Segment::SequenceNumber() const {
    return sequence_number_;
}

void Segment::SetSequenceNumber(uint64_t n) {
  sequence_number_ = n;
}

LivePackager::LivePackager(const LiveConfig &config) 
  : config_(config) {
}

LivePackager::~LivePackager() {}

void LivePackager::Init() {

}

std::unique_ptr<shaka::File, shaka::FileCloser> setupInputSource(const char *fname, 
                                                                const Segment &init, 
                                                                const Segment &segment) {
  std::unique_ptr<File, FileCloser> file(File::Open(fname, "w"));
  file->Write(init.data(), init.size());
  file->Write(segment.data(), segment.size());
  file->Seek(0);

  return file;
}

StreamDescriptors setupStreamDescriptors(const LiveConfig &config,
                                         const BufferCallbackParams &cb_params,
                                         const BufferCallbackParams &init_cb_params) {
  shaka::StreamDescriptor stream_descriptor;

  stream_descriptor.input = 
    shaka::File::MakeCallbackFileName(cb_params, DEFAULT_INPUT_FNAME);

  stream_descriptor.stream_selector = getStreamSelector(config);

  if(LiveConfig::StreamingProtocol::DASH == config.protocol) {
    // init segment
    stream_descriptor.output = 
      shaka::File::MakeCallbackFileName(init_cb_params, DEFAULT_INIT_SEGMENT_FNAME);
  }

  stream_descriptor.segment_template = 
    shaka::File::MakeCallbackFileName(cb_params, getSegmentTemplate(config));

  return StreamDescriptors { stream_descriptor };
} 

Status LivePackager::Package(const Segment &init, const Segment &segment) {
  shaka::BufferCallbackParams callback_params;
  shaka::BufferCallbackParams init_callback_params;

  auto file = setupInputSource(DEFAULT_INPUT_FNAME.c_str(), init, segment);
  callback_params.read_func = [&file](const std::string &name, 
                                      void *buffer,
                                      uint64_t size) {
    // std::cout << "read_func called: size: " << size << std::endl;
    const auto n = file->Read(buffer, size);
    // std::cout << "read size: " << n << std::endl;
    return n;
  };

  std::vector<uint8_t> initBuffer;
  std::vector<uint8_t> segmentBuffer;

  callback_params.write_func = [&segmentBuffer](const std::string &name,
                                                const void *data,
                                                uint64_t size) {
    // std::cout << "write_func called: size: " << size << std::endl;
    auto *ptr = reinterpret_cast<const uint8_t*>(data);
    std::copy(ptr, ptr + size, std::back_inserter(segmentBuffer));
    return size;
  };

  init_callback_params.write_func = [&initBuffer](const std::string &name,
                                                  const void *data,
                                                  uint64_t size) {
    // std::cout << "init_write_func called: size: " << size << std::endl;
    // TODO: this gets called more than once, why?
    // TODO: this is a workaround to write this only once 
    if(initBuffer.size() == 0) {
      auto *ptr = reinterpret_cast<const uint8_t*>(data);
      std::copy(ptr, ptr + size, std::back_inserter(initBuffer));
    }
    return size;
  };

//********************************************************************************

  const std::string fname = 
    shaka::File::MakeCallbackFileName(callback_params, DEFAULT_INPUT_FNAME);

  const bool isHLS = config_.protocol == LiveConfig::StreamingProtocol::HLS;

  const std::string segment_template =
    shaka::File::MakeCallbackFileName(callback_params, getSegmentTemplate(config_));

  const std::string init_segment_cb_fname =
    shaka::File::MakeCallbackFileName(init_callback_params, DEFAULT_INIT_SEGMENT_FNAME);

//********************************************************************************
  shaka::PackagingParams packaging_params;
  packaging_params.chunking_params.segment_duration_in_seconds = config_.segment_duration_in_seconds;

  StreamDescriptors stream_descriptors = setupStreamDescriptors(config_, callback_params, init_callback_params);

//********************************************************************************
  shaka::Packager packager;
  shaka::Status status =
      packager.Initialize(packaging_params, stream_descriptors);

  status = packager.Run();
  ++segment_count_;

//********************************************************************************

  std::cout << "init segment buffer size: " << initBuffer.size() << std::endl;
  std::cout << "segment buffer size: " << segmentBuffer.size() << std::endl;
  if(status == Status::OK) {
    {
      std::stringstream ss;
      ss << std::setw(4) << std::setfill('0') << segment_count_ << (isHLS ? ".ts" : ".m4s");
      std::ofstream fout(ss.str(), std::ios::binary);
      fout.write(reinterpret_cast<const char*>(segmentBuffer.data()), segmentBuffer.size());
      segment_ = shaka::Segment(segmentBuffer.data(), segmentBuffer.size());
    }

    {
      if(segment_count_ == 1 && !initBuffer.empty()) {
        std::ofstream fout(DEFAULT_INIT_SEGMENT_FNAME, std::ios::binary);
        fout.write(reinterpret_cast<const char*>(initBuffer.data()), initBuffer.size());
        init_segment_ = shaka::Segment(initBuffer.data(), initBuffer.size());
      }
    }
  } else {
    std::cout << status.error_message() << std::endl;
  }

  return status;
}

const Segment& LivePackager::GetInitSegment() const {
  return init_segment_;
}
const Segment& LivePackager::GetSegment() const {
  return segment_;
}

}  // namespace shaka
