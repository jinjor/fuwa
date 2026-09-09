#pragma once

#include <string>
#include <vector>

namespace fuwa::audio {

// 既定の出力デバイスに、あらかじめ用意した音を流す。鳴り終わるまで戻らない。
//
// Step 0 の時点ではプラグインをリアルタイムスレッドで回さず、
// 先にレンダリングしたものを再生するだけにしてある。
// リアルタイムコールバックの中でやるのは生のポインタからの書き写しだけなので、
// 確保もロックも起きない。
bool play(const std::vector<std::vector<float>>& channels, double sampleRate, std::string& error);

}  // namespace fuwa::audio
