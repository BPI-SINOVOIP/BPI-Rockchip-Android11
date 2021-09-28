/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "utils/regex-match.h"

#include <memory>

#include "annotator/types.h"

#ifndef TC3_DISABLE_LUA
#include "utils/lua-utils.h"
#ifdef __cplusplus
extern "C" {
#endif
#include "lauxlib.h"
#include "lualib.h"
#ifdef __cplusplus
}
#endif
#endif

namespace libtextclassifier3 {
namespace {

#ifndef TC3_DISABLE_LUA
// Provide a lua environment for running regex match post verification.
// It sets up and exposes the match data as well as the context.
class LuaVerifier : public LuaEnvironment {
 public:
  static std::unique_ptr<LuaVerifier> Create(
      const std::string& context, const std::string& verifier_code,
      const UniLib::RegexMatcher* matcher);

  bool Verify(bool* result);

 private:
  explicit LuaVerifier(const std::string& context,
                       const std::string& verifier_code,
                       const UniLib::RegexMatcher* matcher)
      : context_(context), verifier_code_(verifier_code), matcher_(matcher) {}
  bool Initialize();

  // Provides details of a capturing group to lua.
  int GetCapturingGroup();

  const std::string& context_;
  const std::string& verifier_code_;
  const UniLib::RegexMatcher* matcher_;
};

bool LuaVerifier::Initialize() {
  // Run protected to not lua panic in case of setup failure.
  return RunProtected([this] {
           LoadDefaultLibraries();

           // Expose context of the match as `context` global variable.
           PushString(context_);
           lua_setglobal(state_, "context");

           // Expose match array as `match` global variable.
           // Each entry `match[i]` exposes the ith capturing group as:
           //   * `begin`: span start
           //   * `end`: span end
           //   * `text`: the text
           PushLazyObject(&LuaVerifier::GetCapturingGroup);
           lua_setglobal(state_, "match");
           return LUA_OK;
         }) == LUA_OK;
}

std::unique_ptr<LuaVerifier> LuaVerifier::Create(
    const std::string& context, const std::string& verifier_code,
    const UniLib::RegexMatcher* matcher) {
  auto verifier = std::unique_ptr<LuaVerifier>(
      new LuaVerifier(context, verifier_code, matcher));
  if (!verifier->Initialize()) {
    TC3_LOG(ERROR) << "Could not initialize lua environment.";
    return nullptr;
  }
  return verifier;
}

int LuaVerifier::GetCapturingGroup() {
  if (lua_type(state_, /*idx=*/-1) != LUA_TNUMBER) {
    TC3_LOG(ERROR) << "Unexpected type for match group lookup: "
                   << lua_type(state_, /*idx=*/-1);
    lua_error(state_);
    return 0;
  }
  const int group_id = static_cast<int>(lua_tonumber(state_, /*idx=*/-1));
  int status = UniLib::RegexMatcher::kNoError;
  const CodepointSpan span = {matcher_->Start(group_id, &status),
                              matcher_->End(group_id, &status)};
  std::string text = matcher_->Group(group_id, &status).ToUTF8String();
  if (status != UniLib::RegexMatcher::kNoError) {
    TC3_LOG(ERROR) << "Could not extract span from capturing group.";
    lua_error(state_);
    return 0;
  }
  lua_newtable(state_);
  lua_pushinteger(state_, span.first);
  lua_setfield(state_, /*idx=*/-2, "begin");
  lua_pushinteger(state_, span.second);
  lua_setfield(state_, /*idx=*/-2, "end");
  PushString(text);
  lua_setfield(state_, /*idx=*/-2, "text");
  return 1;
}

bool LuaVerifier::Verify(bool* result) {
  if (luaL_loadbuffer(state_, verifier_code_.data(), verifier_code_.size(),
                      /*name=*/nullptr) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not load verifier snippet.";
    return false;
  }

  if (lua_pcall(state_, /*nargs=*/0, /*nresults=*/1, /*errfunc=*/0) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not run verifier snippet.";
    return false;
  }

  if (RunProtected(
          [this, result] {
            if (lua_type(state_, /*idx=*/-1) != LUA_TBOOLEAN) {
              TC3_LOG(ERROR) << "Unexpected verification result type: "
                             << lua_type(state_, /*idx=*/-1);
              lua_error(state_);
              return LUA_ERRRUN;
            }
            *result = lua_toboolean(state_, /*idx=*/-1);
            return LUA_OK;
          },
          /*num_args=*/1) != LUA_OK) {
    TC3_LOG(ERROR) << "Could not read lua result.";
    return false;
  }
  return true;
}
#endif  // TC3_DISABLE_LUA

}  // namespace

Optional<std::string> GetCapturingGroupText(const UniLib::RegexMatcher* matcher,
                                            const int group_id) {
  int status = UniLib::RegexMatcher::kNoError;
  std::string group_text = matcher->Group(group_id, &status).ToUTF8String();
  if (status != UniLib::RegexMatcher::kNoError || group_text.empty()) {
    return Optional<std::string>();
  }
  return Optional<std::string>(group_text);
}

bool VerifyMatch(const std::string& context,
                 const UniLib::RegexMatcher* matcher,
                 const std::string& lua_verifier_code) {
  bool status = false;
#ifndef TC3_DISABLE_LUA
  auto verifier = LuaVerifier::Create(context, lua_verifier_code, matcher);
  if (verifier == nullptr) {
    TC3_LOG(ERROR) << "Could not create verifier.";
    return false;
  }
  if (!verifier->Verify(&status)) {
    TC3_LOG(ERROR) << "Could not create verifier.";
    return false;
  }
#endif  // TC3_DISABLE_LUA
  return status;
}

}  // namespace libtextclassifier3
