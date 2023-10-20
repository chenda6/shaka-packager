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
#include <cstring>
#include <sstream>
#include <fstream>
#include <filesystem>

#include <packager/live_packager.h>

namespace fs = std::filesystem;

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
  int diff_count(0);
  bool write_outputs(false);

  for(int i(2); i < argc; ++i) {
    std::string fname(argv[i]);
    const std::vector<uint8_t> segment_buff = readSegment(fname.c_str());

    shaka::FullSegment in; 
    in.SetInitSegment(init_buff.data(), init_buff.size());
    in.AppendData(segment_buff.data(), segment_buff.size());

    shaka::FullSegment out;
    const auto status = packager.Package(in, out);
    if(status != shaka::Status::OK) {
      continue;
    }

    const char *data = reinterpret_cast<const char *>(out.GetBuffer().data());
    if(write_outputs) {
      if(i == 2) {
        write("init.mp4", reinterpret_cast<const char *>(data), out.GetInitSegmentSize());
      }
    }

    if(write_outputs) {
      std::stringstream ss;
      ss << std::setw(4) << std::setfill('0') << (i - 1)
        << (config.format == shaka::LiveConfig::OutputFormat::TS ? ".ts" : ".m4s");
      write(ss.str(), data + out.GetInitSegmentSize(), out.GetSegmentSize());
    }

    auto parent_path(fs::path(fname).parent_path());
    std::string expected_fname = parent_path.string() + "/expected/" + std::to_string(i - 1) + ".m4s"; 
    if(!fs::exists(fs::path(expected_fname))) {
      continue;
    }
    const std::vector<uint8_t> expected_buff = readSegment(expected_fname.c_str());

    if(0 != memcmp(expected_buff.data(), data + out.GetInitSegmentSize(), expected_buff.size())) {
      std::cout << fname << "packaged outout is different" << std::endl;
      ++diff_count;
    }
  }

  if(diff_count != 0) {
    std::cout << "Test failed: " << diff_count << " packaged outout(s) are different" << std::endl;
    return 1;
  } else {
    std::cout << "Test passed" << std::endl;
    return 0;
  }
}

int main(int argc, char** argv) {
  // Print the packager version.
  std::cout << "Packager v" + shaka::Packager::GetLibraryVersion() + "\n";
  return testLivePackager(argc, argv);
}
