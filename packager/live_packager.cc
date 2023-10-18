// Copyright 2020 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

#include <fstream>
#include <sstream>

#include <packager/packager.h>
#include <packager/file.h>
#include <packager/file/file_closer.h>
#include <packager/live_packager.h>

namespace shaka {

namespace {

using StreamDescriptors = std::vector<shaka::StreamDescriptor>;

const std::string INPUT_FNAME = "memory://input_file";
const std::string INIT_SEGMENT_FNAME = "init.mp4";

std::string getSegmentTemplate(const LiveConfig &config) {
  return LiveConfig::OutputFormat::TS == config.format ? "$Number$.ts" : "$Number$.m4s";
}

std::string getStreamSelector(const LiveConfig &config) {
  return LiveConfig::TrackType::VIDEO == config.track_type ? "video" : "audio";
}

StreamDescriptors setupStreamDescriptors(const LiveConfig &config,
                                         const BufferCallbackParams &cb_params,
                                         const BufferCallbackParams &init_cb_params) {
  shaka::StreamDescriptor desc;

  desc.input = File::MakeCallbackFileName(cb_params, INPUT_FNAME);

  desc.stream_selector = getStreamSelector(config);

  if(LiveConfig::OutputFormat::FMP4 == config.format) {
    // init segment
    desc.output = File::MakeCallbackFileName(init_cb_params, INIT_SEGMENT_FNAME);
  }

  desc.segment_template =
    File::MakeCallbackFileName(cb_params, getSegmentTemplate(config));

  return StreamDescriptors { desc };
} 

} // namespace

Segment::Segment(const uint8_t *data, size_t size) 
  : data_(data, data + size) {
}

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

LivePackager::LivePackager(const LiveConfig &config) 
  : config_(config) {
}

LivePackager::~LivePackager() {
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

Status LivePackager::Package(const FullSegment &in, FullSegment &out) {
  auto file = setupInputSource(INPUT_FNAME.c_str(), in.init, in.data);

  std::vector<uint8_t> initBuffer;
  std::vector<uint8_t> segmentBuffer;

  shaka::BufferCallbackParams callback_params;
  callback_params.read_func = [&file](const std::string &name, 
                                      void *buffer,
                                      uint64_t size) {
    // TODO: replace with actual logging
    // std::cout << "read_func called: size: " << size << std::endl;
    const auto n = file->Read(buffer, size);
    // std::cout << "read size: " << n << std::endl;
    return n;
  };

  callback_params.write_func = [&segmentBuffer](const std::string &name,
                                                const void *data,
                                                uint64_t size) {
    // TODO: replace with actual logging
    // std::cout << "write_func called: size: " << size << std::endl;
    auto *ptr = reinterpret_cast<const uint8_t*>(data);
    std::copy(ptr, ptr + size, std::back_inserter(segmentBuffer));
    return size;
  };

  shaka::BufferCallbackParams init_callback_params;
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

  shaka::PackagingParams params;
  params.chunking_params.segment_duration_in_seconds = config_.segment_duration_sec;

  StreamDescriptors descriptors =
    setupStreamDescriptors(config_, callback_params, init_callback_params);

  shaka::Packager packager;
  shaka::Status status = packager.Initialize(params, descriptors);

  if(status != Status::OK) {
    return status;
  }      

  status = packager.Run();
  ++segment_count_;

  if(status == Status::OK) {
    // TODO: avoid second copy 
    out.data = shaka::Segment(segmentBuffer.data(), segmentBuffer.size());
    out.init = shaka::Segment(initBuffer.data(), initBuffer.size());
  }

  return status;
}

}  // namespace shaka
