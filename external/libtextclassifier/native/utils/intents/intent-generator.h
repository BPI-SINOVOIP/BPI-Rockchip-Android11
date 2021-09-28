
#ifndef LIBTEXTCLASSIFIER_UTILS_INTENTS_INTENT_GENERATOR_H_
#define LIBTEXTCLASSIFIER_UTILS_INTENTS_INTENT_GENERATOR_H_

#include <jni.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "actions/types.h"
#include "annotator/types.h"
#include "utils/i18n/locale.h"
#include "utils/intents/intent-config_generated.h"
#include "utils/intents/remote-action-template.h"
#include "utils/java/jni-cache.h"
#include "utils/resources.h"
#include "utils/resources_generated.h"
#include "utils/strings/stringpiece.h"

namespace libtextclassifier3 {

// Helper class to generate Android intents for text classifier results.
class IntentGenerator {
 public:
  static std::unique_ptr<IntentGenerator> Create(
      const IntentFactoryModel* options, const ResourcePool* resources,
      const std::shared_ptr<JniCache>& jni_cache);

  // Generates intents for a classification result.
  // Returns true, if the intent generator snippets could be successfully run,
  // returns false otherwise.
  bool GenerateIntents(const jstring device_locales,
                       const ClassificationResult& classification,
                       const int64 reference_time_ms_utc,
                       const std::string& text,
                       const CodepointSpan selection_indices,
                       const jobject context,
                       const reflection::Schema* annotations_entity_data_schema,
                       std::vector<RemoteActionTemplate>* remote_actions) const;

  // Generates intents for an action suggestion.
  // Returns true, if the intent generator snippets could be successfully run,
  // returns false otherwise.
  bool GenerateIntents(const jstring device_locales,
                       const ActionSuggestion& action,
                       const Conversation& conversation, const jobject context,
                       const reflection::Schema* annotations_entity_data_schema,
                       const reflection::Schema* actions_entity_data_schema,
                       std::vector<RemoteActionTemplate>* remote_actions) const;

 private:
  IntentGenerator(const IntentFactoryModel* options,
                  const ResourcePool* resources,
                  const std::shared_ptr<JniCache>& jni_cache)
      : options_(options),
        resources_(Resources(resources)),
        jni_cache_(jni_cache) {}

  std::vector<Locale> ParseDeviceLocales(const jstring device_locales) const;

  const IntentFactoryModel* options_;
  const Resources resources_;
  std::shared_ptr<JniCache> jni_cache_;
  std::map<std::string, std::string> generators_;
};

}  // namespace libtextclassifier3

#endif  // LIBTEXTCLASSIFIER_UTILS_INTENTS_INTENT_GENERATOR_H_
