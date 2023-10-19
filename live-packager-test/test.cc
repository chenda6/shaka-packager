// Copyright 2023 Google LLC. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file or at
// https://developers.google.com/open-source/licenses/bsd

// This is a simple app to test linking against a shared libpackager on all
// platforms.  It's not meant to do anything useful at all.

#include <cstdio>
#include <vector>
#include <string>
#include <sstream>
#include <fstream>

#include <packager/live_packager.h>

void write(const std::string &fname, const char *data, size_t size) {
  if(size == 0) {
    return;
  }
  std::ofstream fout(fname, std::ios::binary);
  fout.write(data, size);
}

std::vector<uint8_t> readSegment(const char *fname) {
  std::cout << "reading: " << fname << std::endl;
  std::vector<uint8_t> data;

  try {
    std::ifstream fin(fname, std::ios::binary);
    fin.seekg(0, std::ios::end);
    data.resize(fin.tellg());
    fin.seekg(0, std::ios::beg);
    fin.read(reinterpret_cast<char *>(data.data()), data.size());
  } catch(std::exception &e) {
    std::cerr << e.what() << std::endl;
  }
  return data;
}

int testLivePackager(int argc, char **argv) {
  std::vector<uint8_t> init_buff = readSegment(argv[1]);

  shaka::LiveConfig config {
    .format = shaka::LiveConfig::OutputFormat::TS,
    .segment_duration_sec = 5,
    .track_type = shaka::LiveConfig::TrackType::VIDEO
  };

  shaka::LivePackager packager(config);
  for(int i(2); i < argc; ++i) {
    const std::vector<uint8_t> segment_buff = readSegment(argv[i]);

    shaka::FullSegment in; 
    in.SetInitSegment(init_buff.data(), init_buff.size());
    in.AppendData(segment_buff.data(), segment_buff.size());

    shaka::FullSegment out;
    const auto status = packager.Package(in, out);
    if(status != shaka::Status::OK) {
      continue;
    }

    const char *data = reinterpret_cast<const char *>(out.GetBuffer().data());
    if(i == 2) {
      write("init.mp4", reinterpret_cast<const char *>(data), out.GetInitSegmentSize());
    }

    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << (i - 1)
       << (config.format == shaka::LiveConfig::OutputFormat::TS ? ".ts" : ".m4s");
    write(ss.str(), data + out.GetInitSegmentSize(), out.GetSegmentSize());
  }

  return 0;
}

int main(int argc, char** argv) {
  // Print the packager version.
  std::cout << "Packager v" + shaka::Packager::GetLibraryVersion() + "\n";
  return testLivePackager(argc, argv);
}
