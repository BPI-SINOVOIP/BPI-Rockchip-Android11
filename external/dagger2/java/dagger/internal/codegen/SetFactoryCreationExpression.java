/*
 * Copyright (C) 2015 The Dagger Authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package dagger.internal.codegen;

import static com.google.common.base.Preconditions.checkNotNull;
import static dagger.internal.codegen.SourceFiles.setFactoryClassName;

import com.squareup.javapoet.CodeBlock;
import dagger.model.DependencyRequest;
import dagger.producers.Produced;
import java.util.Optional;

/** A factory creation expression for a multibound set. */
final class SetFactoryCreationExpression extends MultibindingFactoryCreationExpression {

  private final ComponentImplementation componentImplementation;
  private final BindingGraph graph;
  private final ContributionBinding binding;

  SetFactoryCreationExpression(
      ContributionBinding binding,
      ComponentImplementation componentImplementation,
      ComponentBindingExpressions componentBindingExpressions,
      BindingGraph graph) {
    super(binding, componentImplementation, componentBindingExpressions);
    this.binding = checkNotNull(binding);
    this.componentImplementation = checkNotNull(componentImplementation);
    this.graph = checkNotNull(graph);
  }

  @Override
  public CodeBlock creationExpression() {
    CodeBlock.Builder builder = CodeBlock.builder().add("$T.", setFactoryClassName(binding));
    if (!useRawType()) {
      SetType setType = SetType.from(binding.key());
      builder.add(
          "<$T>",
          setType.elementsAreTypeOf(Produced.class)
              ? setType.unwrappedElementType(Produced.class)
              : setType.elementType());
    }

    int individualProviders = 0;
    int setProviders = 0;
    CodeBlock.Builder builderMethodCalls = CodeBlock.builder();
    String methodNameSuffix =
        binding.bindingType().equals(BindingType.PROVISION) ? "Provider" : "Producer";

    Optional<CodeBlock> superContributions = superContributions();
    if (superContributions.isPresent()) {
      // TODO(b/117833324): consider decomposing the Provider<Set<Provider>> and adding the
      // individual contributions separately from the collection contributions. Though this may
      // actually not be doable/desirable if the super provider instance is a DelegateFactory or
      // another internal type that is not SetFactory
      builderMethodCalls.add(".addCollection$N($L)", methodNameSuffix, superContributions.get());
      setProviders++;
    }

    for (DependencyRequest dependency : dependenciesToImplement()) {
      ContributionType contributionType =
          graph.contributionBindings().get(dependency.key()).contributionType();
      String methodNamePrefix;
      switch (contributionType) {
        case SET:
          individualProviders++;
          methodNamePrefix = "add";
          break;
        case SET_VALUES:
          setProviders++;
          methodNamePrefix = "addCollection";
          break;
        default:
          throw new AssertionError(dependency + " is not a set multibinding");
      }

      builderMethodCalls.add(
          ".$N$N($L)",
          methodNamePrefix,
          methodNameSuffix,
          multibindingDependencyExpression(dependency));
    }
    builder.add("builder($L, $L)", individualProviders, setProviders);
    builder.add(builderMethodCalls.build());

    componentImplementation.registerImplementedMultibinding(binding, bindingRequest());

    return builder.add(".build()").build();
  }
}
