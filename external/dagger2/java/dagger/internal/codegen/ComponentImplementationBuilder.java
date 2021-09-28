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

import static com.google.auto.common.MoreElements.getLocalAndInheritedMethods;
import static com.google.auto.common.MoreTypes.asDeclared;
import static com.google.common.base.Preconditions.checkArgument;
import static com.google.common.base.Preconditions.checkState;
import static com.google.common.base.Predicates.in;
import static com.squareup.javapoet.MethodSpec.constructorBuilder;
import static com.squareup.javapoet.MethodSpec.methodBuilder;
import static dagger.internal.codegen.BindingRequest.bindingRequest;
import static dagger.internal.codegen.ComponentCreatorKind.BUILDER;
import static dagger.internal.codegen.ComponentImplementation.MethodSpecKind.BUILDER_METHOD;
import static dagger.internal.codegen.ComponentImplementation.MethodSpecKind.CANCELLATION_LISTENER_METHOD;
import static dagger.internal.codegen.ComponentImplementation.MethodSpecKind.COMPONENT_METHOD;
import static dagger.internal.codegen.ComponentImplementation.MethodSpecKind.CONSTRUCTOR;
import static dagger.internal.codegen.ComponentImplementation.MethodSpecKind.INITIALIZE_METHOD;
import static dagger.internal.codegen.ComponentImplementation.MethodSpecKind.MODIFIABLE_BINDING_METHOD;
import static dagger.internal.codegen.ComponentImplementation.TypeSpecKind.COMPONENT_CREATOR;
import static dagger.internal.codegen.ComponentImplementation.TypeSpecKind.SUBCOMPONENT;
import static dagger.internal.codegen.DaggerStreams.toImmutableList;
import static dagger.internal.codegen.javapoet.AnnotationSpecs.Suppression.UNCHECKED;
import static dagger.internal.codegen.javapoet.CodeBlocks.parameterNames;
import static dagger.internal.codegen.javapoet.CodeBlocks.toParametersCodeBlock;
import static dagger.producers.CancellationPolicy.Propagation.PROPAGATE;
import static javax.lang.model.element.Modifier.ABSTRACT;
import static javax.lang.model.element.Modifier.FINAL;
import static javax.lang.model.element.Modifier.PRIVATE;
import static javax.lang.model.element.Modifier.PROTECTED;
import static javax.lang.model.element.Modifier.PUBLIC;
import static javax.lang.model.element.Modifier.STATIC;

import com.google.auto.common.MoreTypes;
import com.google.common.collect.ImmutableList;
import com.google.common.collect.ImmutableListMultimap;
import com.google.common.collect.ImmutableMap;
import com.google.common.collect.ImmutableSet;
import com.google.common.collect.Iterables;
import com.google.common.collect.Lists;
import com.google.common.collect.Maps;
import com.google.common.collect.Multimaps;
import com.google.common.collect.Sets;
import com.squareup.javapoet.AnnotationSpec;
import com.squareup.javapoet.ClassName;
import com.squareup.javapoet.CodeBlock;
import com.squareup.javapoet.MethodSpec;
import com.squareup.javapoet.ParameterSpec;
import com.squareup.javapoet.TypeName;
import com.squareup.javapoet.TypeSpec;
import dagger.internal.ComponentDefinitionType;
import dagger.internal.Preconditions;
import dagger.internal.codegen.ComponentDescriptor.ComponentMethodDescriptor;
import dagger.internal.codegen.ComponentImplementation.ConfigureInitializationMethod;
import dagger.internal.codegen.ModifiableBindingMethods.ModifiableBindingMethod;
import dagger.internal.codegen.javapoet.AnnotationSpecs;
import dagger.internal.codegen.javapoet.CodeBlocks;
import dagger.internal.codegen.langmodel.DaggerElements;
import dagger.internal.codegen.langmodel.DaggerTypes;
import dagger.model.Key;
import dagger.producers.internal.CancellationListener;
import dagger.producers.internal.Producers;
import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.function.Function;
import javax.inject.Inject;
import javax.lang.model.element.ExecutableElement;
import javax.lang.model.type.DeclaredType;

/** A builder of {@link ComponentImplementation}s. */
abstract class ComponentImplementationBuilder {
  private static final String MAY_INTERRUPT_IF_RUNNING = "mayInterruptIfRunning";

  /**
   * How many statements per {@code initialize()} or {@code onProducerFutureCancelled()} method
   * before they get partitioned.
   */
  private static final int STATEMENTS_PER_METHOD = 100;

  private static final String CANCELLATION_LISTENER_METHOD_NAME = "onProducerFutureCancelled";

  // TODO(ronshapiro): replace this with composition instead of inheritance so we don't have
  // non-final fields
  @Inject BindingGraph graph;
  @Inject ComponentBindingExpressions bindingExpressions;
  @Inject ComponentRequirementExpressions componentRequirementExpressions;
  @Inject ComponentImplementation componentImplementation;
  @Inject ComponentCreatorImplementationFactory componentCreatorImplementationFactory;
  @Inject DaggerTypes types;
  @Inject DaggerElements elements;
  @Inject CompilerOptions compilerOptions;
  @Inject ComponentImplementationFactory componentImplementationFactory;
  @Inject TopLevelImplementationComponent topLevelImplementationComponent;
  private boolean done;

  /**
   * Returns a {@link ComponentImplementation} for this component. This is only intended to be
   * called once (and will throw on successive invocations). If the component must be regenerated,
   * use a new instance.
   */
  final ComponentImplementation build() {
    checkState(
        !done,
        "ComponentImplementationBuilder has already built the ComponentImplementation for [%s].",
        componentImplementation.name());
    setSupertype();
    componentImplementation.setCreatorImplementation(
        componentCreatorImplementationFactory.create(
            componentImplementation, Optional.of(componentImplementation.graph())));
    componentImplementation
        .creatorImplementation()
        .map(ComponentCreatorImplementation::spec)
        .ifPresent(this::addCreatorClass);

    getLocalAndInheritedMethods(graph.componentTypeElement(), types, elements)
        .forEach(method -> componentImplementation.claimMethodName(method.getSimpleName()));
    componentImplementation
        .superclassImplementation()
        .ifPresent(
            superclassImplementation -> {
              superclassImplementation
                  .getAllModifiableMethodNames()
                  .forEach(componentImplementation::claimMethodName);
            });

    addFactoryMethods();
    addInterfaceMethods();
    addChildComponents();
    implementModifiableModuleMethods();

    addConstructorAndInitializationMethods();

    if (graph.componentDescriptor().isProduction()) {
      addCancellationListenerImplementation();
    }

    if (componentImplementation.isAbstract()
        && !componentImplementation.baseImplementation().isPresent()) {
      componentImplementation.addAnnotation(compilerOptions.toGenerationOptionsAnnotation());
    }

    if (componentImplementation.shouldEmitModifiableMetadataAnnotations()) {
      componentImplementation.addAnnotation(
          AnnotationSpec.builder(ComponentDefinitionType.class)
              .addMember("value", "$T.class", graph.componentTypeElement())
              .build());
    }

    done = true;
    return componentImplementation;
  }

  /** Set the supertype for this generated class. */
  private void setSupertype() {
    if (componentImplementation.superclassImplementation().isPresent()) {
      componentImplementation.addSuperclass(
          componentImplementation.superclassImplementation().get().name());
    } else {
      componentImplementation.addSupertype(graph.componentTypeElement());
    }
  }

  /**
   * Adds {@code creator} as a nested creator class. Root components and subcomponents will nest
   * this in different classes.
   */
  protected abstract void addCreatorClass(TypeSpec creator);

  /** Adds component factory methods. */
  protected abstract void addFactoryMethods();

  protected void addInterfaceMethods() {
    // Each component method may have been declared by several supertypes. We want to implement
    // only one method for each distinct signature.
    ImmutableListMultimap<MethodSignature, ComponentMethodDescriptor> componentMethodsBySignature =
        Multimaps.index(graph.componentDescriptor().entryPointMethods(), this::getMethodSignature);
    for (List<ComponentMethodDescriptor> methodsWithSameSignature :
        Multimaps.asMap(componentMethodsBySignature).values()) {
      ComponentMethodDescriptor anyOneMethod = methodsWithSameSignature.stream().findAny().get();
      MethodSpec methodSpec = bindingExpressions.getComponentMethod(anyOneMethod);

      if (compilerOptions.aheadOfTimeSubcomponents()) {
        addPossiblyModifiableInterfaceMethod(anyOneMethod, methodSpec);
      } else {
        componentImplementation.addMethod(COMPONENT_METHOD, methodSpec);
      }
    }
  }

  /**
   * Adds a component interface method in ahead-of-time subcomponents mode. If the binding that
   * implements the method is modifiable, registers the method.
   */
  private void addPossiblyModifiableInterfaceMethod(
      ComponentMethodDescriptor methodDescriptor, MethodSpec implementedComponentMethod) {
    if (methodDescriptor.dependencyRequest().isPresent()
        && componentImplementation
            .getModifiableBindingMethod(bindingRequest(methodDescriptor.dependencyRequest().get()))
            .isPresent()) {
      // If there are multiple component methods that are modifiable and for the same binding
      // request, implement all but one in the base implementation to delegate to the one that
      // will remain (and be registered) modifiable
      checkState(componentImplementation.isAbstract() && !componentImplementation.isNested());
      componentImplementation.addMethod(
          COMPONENT_METHOD, implementedComponentMethod.toBuilder().addModifiers(FINAL).build());
    } else {
      // TODO(b/117833324): Can this class be the one to interface with ComponentImplementation
      // instead of having it go through ModifiableBindingExpressions?
      bindingExpressions
          .modifiableBindingExpressions()
          .addPossiblyModifiableComponentMethod(methodDescriptor, implementedComponentMethod);
    }
  }

  private void addCancellationListenerImplementation() {
    componentImplementation.addSupertype(elements.getTypeElement(CancellationListener.class));
    componentImplementation.claimMethodName(CANCELLATION_LISTENER_METHOD_NAME);

    ImmutableList<ParameterSpec> parameters =
        ImmutableList.of(ParameterSpec.builder(boolean.class, MAY_INTERRUPT_IF_RUNNING).build());

    MethodSpec.Builder methodBuilder =
        methodBuilder(CANCELLATION_LISTENER_METHOD_NAME)
            .addModifiers(PUBLIC)
            .addAnnotation(Override.class)
            .addParameters(parameters);
    if (componentImplementation.superclassImplementation().isPresent()) {
      methodBuilder.addStatement(
          "super.$L($L)", CANCELLATION_LISTENER_METHOD_NAME, MAY_INTERRUPT_IF_RUNNING);
    }

    ImmutableList<CodeBlock> cancellationStatements = cancellationStatements();

    if (cancellationStatements.size() < STATEMENTS_PER_METHOD) {
      methodBuilder.addCode(CodeBlocks.concat(cancellationStatements)).build();
    } else {
      ImmutableList<MethodSpec> cancelProducersMethods =
          createPartitionedMethods(
              "cancelProducers",
              parameters,
              cancellationStatements,
              methodName -> methodBuilder(methodName).addModifiers(PRIVATE));
      for (MethodSpec cancelProducersMethod : cancelProducersMethods) {
        methodBuilder.addStatement("$N($L)", cancelProducersMethod, MAY_INTERRUPT_IF_RUNNING);
        componentImplementation.addMethod(CANCELLATION_LISTENER_METHOD, cancelProducersMethod);
      }
    }

    Optional<CodeBlock> cancelParentStatement = cancelParentStatement();
    cancelParentStatement.ifPresent(methodBuilder::addCode);

    if (cancellationStatements.isEmpty()
        && !cancelParentStatement.isPresent()
        && componentImplementation.superclassImplementation().isPresent()) {
      // Partial child implementations that have no new cancellations don't need to override
      // the method just to call super().
      return;
    }

    componentImplementation.addMethod(CANCELLATION_LISTENER_METHOD, methodBuilder.build());
  }

  private ImmutableList<CodeBlock> cancellationStatements() {
    // Reversing should order cancellations starting from entry points and going down to leaves
    // rather than the other way around. This shouldn't really matter but seems *slightly*
    // preferable because:
    // When a future that another future depends on is cancelled, that cancellation will propagate
    // up the future graph toward the entry point. Cancelling in reverse order should ensure that
    // everything that depends on a particular node has already been cancelled when that node is
    // cancelled, so there's no need to propagate. Otherwise, when we cancel a leaf node, it might
    // propagate through most of the graph, making most of the cancel calls that follow in the
    // onProducerFutureCancelled method do nothing.
    ImmutableList<Key> cancellationKeys =
        componentImplementation.getCancellableProducerKeys().reverse();

    ImmutableList.Builder<CodeBlock> cancellationStatements = ImmutableList.builder();
    for (Key cancellationKey : cancellationKeys) {
      cancellationStatements.add(
          CodeBlock.of(
              "$T.cancel($L, $N);",
              Producers.class,
              bindingExpressions
                  .getDependencyExpression(
                      bindingRequest(cancellationKey, FrameworkType.PRODUCER_NODE),
                      componentImplementation.name())
                  .codeBlock(),
              MAY_INTERRUPT_IF_RUNNING));
    }
    return cancellationStatements.build();
  }

  protected Optional<CodeBlock> cancelParentStatement() {
    // Returns empty by default. Overridden in subclass(es) to add a statement if and only if the
    // component being generated is a concrete subcomponent implementation with a parent that
    // allows cancellation to propagate to it from subcomponents.
    return Optional.empty();
  }

  /**
   * For final components, reimplements all modifiable module methods that may have been modified.
   */
  private void implementModifiableModuleMethods() {
    if (componentImplementation.isAbstract()) {
      return;
    }
    componentImplementation
        .getAllModifiableModuleMethods()
        .forEach(this::implementModifiableModuleMethod);
  }

  private void implementModifiableModuleMethod(ComponentRequirement module, String methodName) {
    // TODO(b/117833324): only reimplement methods for modules that were abstract or were repeated
    // by an ancestor component.
    componentImplementation.addMethod(
        MODIFIABLE_BINDING_METHOD,
        methodBuilder(methodName)
            .addAnnotation(Override.class)
            .addModifiers(PROTECTED)
            .returns(TypeName.get(module.type()))
            .addStatement(
                componentRequirementExpressions
                    .getExpression(module)
                    .getModifiableModuleMethodExpression(componentImplementation.name()))
            .build());
  }

  private MethodSignature getMethodSignature(ComponentMethodDescriptor method) {
    return MethodSignature.forComponentMethod(
        method, MoreTypes.asDeclared(graph.componentTypeElement().asType()), types);
  }

  private void addChildComponents() {
    for (BindingGraph subgraph : graph.subgraphs()) {
      // TODO(b/117833324): Can an abstract inner subcomponent implementation be elided if it's
      // totally empty?
      componentImplementation.addChild(
          subgraph.componentDescriptor(), buildChildImplementation(subgraph));
    }
  }

  private ComponentImplementation buildChildImplementation(BindingGraph childGraph) {
    ComponentImplementation childImplementation =
        compilerOptions.aheadOfTimeSubcomponents()
            ? abstractInnerSubcomponent(childGraph)
            : concreteSubcomponent(childGraph);
    return topLevelImplementationComponent
        .currentImplementationSubcomponentBuilder()
        .componentImplementation(childImplementation)
        .bindingGraph(childGraph)
        .parentBuilder(Optional.of(this))
        .parentBindingExpressions(Optional.of(bindingExpressions))
        .parentRequirementExpressions(Optional.of(componentRequirementExpressions))
        .build()
        .subcomponentBuilder()
        .build();
  }

  /** Creates an inner abstract subcomponent implementation. */
  private ComponentImplementation abstractInnerSubcomponent(BindingGraph childGraph) {
    return componentImplementation.childComponentImplementation(
        childGraph,
        Optional.of(
            componentImplementationFactory.findChildSuperclassImplementation(
                childGraph.componentDescriptor(), componentImplementation)),
        PROTECTED,
        componentImplementation.isAbstract() ? ABSTRACT : FINAL);
  }

  /** Creates a concrete inner subcomponent implementation. */
  private ComponentImplementation concreteSubcomponent(BindingGraph childGraph) {
    return componentImplementation.childComponentImplementation(
        childGraph,
        Optional.empty(), // superclassImplementation
        PRIVATE,
        FINAL);
  }

  /** Creates and adds the constructor and methods needed for initializing the component. */
  private void addConstructorAndInitializationMethods() {
    MethodSpec.Builder constructor = componentConstructorBuilder();
    if (!componentImplementation.isAbstract()) {
      implementInitializationMethod(constructor, initializationParameters());
    } else if (componentImplementation.hasInitializations()) {
      addConfigureInitializationMethod();
    }
    componentImplementation.addMethod(CONSTRUCTOR, constructor.build());
  }

  /** Returns a builder for the component's constructor. */
  private MethodSpec.Builder componentConstructorBuilder() {
    return constructorBuilder()
            .addModifiers(componentImplementation.isAbstract() ? PROTECTED : PRIVATE);
  }

  /** Adds parameters and code to the given {@code initializationMethod}. */
  private void implementInitializationMethod(
      MethodSpec.Builder initializationMethod,
      ImmutableMap<ComponentRequirement, ParameterSpec> initializationParameters) {
    initializationMethod.addParameters(initializationParameters.values());
    initializationMethod.addCode(
        CodeBlocks.concat(componentImplementation.getComponentRequirementInitializations()));
    componentImplementation
        .superConfigureInitializationMethod()
        .ifPresent(
            superConfigureInitializationMethod ->
                addSuperConfigureInitializationCall(
                    initializationMethod,
                    initializationParameters,
                    superConfigureInitializationMethod));
    addInitializeMethods(initializationMethod, initializationParameters.values().asList());
  }

  /** Creates and adds a {@code configureInitializatoin} method to the component. */
  private void addConfigureInitializationMethod() {
    MethodSpec.Builder method = configureInitializationMethodBuilder();
    ImmutableMap<ComponentRequirement, ParameterSpec> parameters = initializationParameters();
    implementInitializationMethod(method, parameters);
    componentImplementation.setConfigureInitializationMethod(
        ConfigureInitializationMethod.create(method.build(), parameters.keySet()));
  }

  /** Returns a builder for the component's {@code configureInitialization} method. */
  private MethodSpec.Builder configureInitializationMethodBuilder() {
    String methodName = componentImplementation.getUniqueMethodName("configureInitialization");
    MethodSpec.Builder configureInitialization = methodBuilder(methodName).addModifiers(PROTECTED);
    if (overridesSuperclassConfigureInitialization(configureInitialization.build())) {
      configureInitialization.addAnnotation(Override.class);
    }
    return configureInitialization;
  }

  /**
   * Returns whether or not the given method overrides a configureInitialization method from a
   * superclass.
   */
  private boolean overridesSuperclassConfigureInitialization(MethodSpec method) {
    for (Optional<ComponentImplementation> currentSuperImplementation =
            componentImplementation.superclassImplementation();
        currentSuperImplementation.isPresent();
        currentSuperImplementation = currentSuperImplementation.get().superclassImplementation()) {
      Optional<MethodSpec> superConfigureInitializationMethod =
          currentSuperImplementation.get().configureInitializationMethod().map(m -> m.spec());
      if (superConfigureInitializationMethod
          .filter(superMethod -> haveSameSignature(method, superMethod))
          .isPresent()) {
        return true;
      }
    }

    return false;
  }

  /** Returns whether or not methods {@code a} and {@code b} have the same signature. */
  private boolean haveSameSignature(MethodSpec a, MethodSpec b) {
    return a.name.equals(b.name) && types(a.parameters).equals(types(b.parameters));
  }

  private ImmutableList<TypeName> types(List<ParameterSpec> parameters) {
    return parameters.stream().map(parameter -> parameter.type).collect(toImmutableList());
  }

  /**
   * Adds a call to the superclass's {@code configureInitialization} method to the given {@code
   * callingMethod}.
   */
  private void addSuperConfigureInitializationCall(
      MethodSpec.Builder callingMethod,
      ImmutableMap<ComponentRequirement, ParameterSpec> parameters,
      ConfigureInitializationMethod superConfigureInitializationMethod) {
    // This component's constructor may not have all of the parameters that the superclass's
    // configureInitialization method takes, because the superclass configureInitialization method
    // necessarily accepts things that it can't know whether will be needed or not. If they aren't
    // needed (as is the case when the constructor doesn't have a parameter for the module), just
    // pass null to super.configureInitialization for that parameter; it won't be used.
    CodeBlock args =
        superConfigureInitializationMethod.parameters().stream()
            .map(
                requirement ->
                    parameters.containsKey(requirement)
                        ? CodeBlock.of("$N", parameters.get(requirement))
                        : CodeBlock.of("null"))
            .collect(toParametersCodeBlock());

    String qualifier =
        haveSameSignature(callingMethod.build(), superConfigureInitializationMethod.spec())
            ? "super."
            : "";
    callingMethod.addStatement(
        qualifier + "$N($L)", superConfigureInitializationMethod.spec(), args);
  }

  /**
   * Adds any necessary {@code initialize} methods to the component and adds calls to them to the
   * given {@code callingMethod}.
   */
  private void addInitializeMethods(
      MethodSpec.Builder callingMethod, ImmutableList<ParameterSpec> parameters) {
    // TODO(cgdecker): It's not the case that each initialize() method has need for all of the
    // given parameters. In some cases, those parameters may have already been assigned to fields
    // which could be referenced instead. In other cases, an initialize method may just not need
    // some of the parameters because the set of initializations in that partition does not
    // include any reference to them. Right now, the Dagger code has no way of getting that
    // information because, among other things, componentImplementation.getImplementations() just
    // returns a bunch of CodeBlocks with no semantic information. Additionally, we may not know
    // yet whether a field will end up needing to be created for a specific requirement, and we
    // don't want to create a field that ends up only being used during initialization.
    CodeBlock args = parameterNames(parameters);
    ImmutableList<MethodSpec> methods =
        createPartitionedMethods(
            "initialize",
            makeFinal(parameters),
            componentImplementation.getInitializations(),
            methodName ->
                methodBuilder(methodName)
                    .addModifiers(PRIVATE)
                    /* TODO(gak): Strictly speaking, we only need the suppression here if we are
                     * also initializing a raw field in this method, but the structure of this
                     * code makes it awkward to pass that bit through.  This will be cleaned up
                     * when we no longer separate fields and initialization as we do now. */
                    .addAnnotation(AnnotationSpecs.suppressWarnings(UNCHECKED)));
    for (MethodSpec method : methods) {
      callingMethod.addStatement("$N($L)", method, args);
      componentImplementation.addMethod(INITIALIZE_METHOD, method);
    }
  }

  /**
   * Creates one or more methods, all taking the given {@code parameters}, which partition the given
   * list of {@code statements} among themselves such that no method has more than {@code
   * STATEMENTS_PER_METHOD} statements in it and such that the returned methods, if called in order,
   * will execute the {@code statements} in the given order.
   */
  private ImmutableList<MethodSpec> createPartitionedMethods(
      String methodName,
      Iterable<ParameterSpec> parameters,
      List<CodeBlock> statements,
      Function<String, MethodSpec.Builder> methodBuilderCreator) {
    return Lists.partition(statements, STATEMENTS_PER_METHOD).stream()
        .map(
            partition ->
                methodBuilderCreator
                    .apply(componentImplementation.getUniqueMethodName(methodName))
                    .addParameters(parameters)
                    .addCode(CodeBlocks.concat(partition))
                    .build())
        .collect(toImmutableList());
  }

  /** Returns the given parameters with a final modifier added. */
  private final ImmutableList<ParameterSpec> makeFinal(Collection<ParameterSpec> parameters) {
    return parameters.stream()
        .map(param -> param.toBuilder().addModifiers(FINAL).build())
        .collect(toImmutableList());
  }

  /**
   * Returns the parameters for the constructor or {@code configureInitilization} method as a map
   * from the requirement the parameter fulfills to the spec for the parameter.
   */
  private final ImmutableMap<ComponentRequirement, ParameterSpec> initializationParameters() {
    Map<ComponentRequirement, ParameterSpec> parameters;
    if (componentImplementation.componentDescriptor().hasCreator()) {
      parameters =
          Maps.toMap(componentImplementation.requirements(), ComponentRequirement::toParameterSpec);
    } else if (componentImplementation.isAbstract() && componentImplementation.isNested()) {
      // If we're generating an abstract inner subcomponent, then we are not implementing module
      // instance bindings and have no need for factory method parameters.
      parameters = ImmutableMap.of();
    } else if (graph.factoryMethod().isPresent()) {
      parameters = getFactoryMethodParameters(graph);
    } else if (componentImplementation.isAbstract()) {
      // If we're generating an abstract base implementation of a subcomponent it's acceptable to
      // have neither a creator nor factory method.
      parameters = ImmutableMap.of();
    } else {
      throw new AssertionError(
          "Expected either a component creator or factory method but found neither.");
    }

    if (componentImplementation.isAbstract()) {
      parameters = Maps.filterKeys(parameters, in(configureInitializationRequirements()));
    }
    return renameParameters(parameters);
  }

  /**
   * Returns the set of requirements for the configureInitialization method: the parameters that are
   * needed either for initializing a component requirement field or for calling the superclass's
   * {@code configureInitialization} method.
   */
  private ImmutableSet<ComponentRequirement> configureInitializationRequirements() {
    ImmutableSet<ComponentRequirement> initializationParameters =
        componentImplementation.getComponentRequirementParameters();
    ImmutableSet<ComponentRequirement> superConfigureInitializationRequirements =
        componentImplementation
            .superConfigureInitializationMethod()
            .map(ConfigureInitializationMethod::parameters)
            .orElse(ImmutableSet.of());
    return Sets.union(initializationParameters, superConfigureInitializationRequirements)
        .immutableCopy();
  }

  /**
   * Renames the given parameters to guarantee their names do not conflict with fields in the
   * component to ensure that a parameter is never referenced where a reference to a field was
   * intended.
   */
  // TODO(cgdecker): This is a bit kludgy; it would be preferable to either qualify the field
  // references with "this." or "super." when needed to disambiguate between field and parameter,
  // but that would require more context than is currently available when the code referencing a
  // field is generated.
  private ImmutableMap<ComponentRequirement, ParameterSpec> renameParameters(
      Map<ComponentRequirement, ParameterSpec> parameters) {
    return ImmutableMap.copyOf(
        Maps.transformEntries(
            parameters,
            (requirement, parameter) ->
                renameParameter(
                    parameter,
                    componentImplementation.getParameterName(requirement, parameter.name))));
  }

  private ParameterSpec renameParameter(ParameterSpec parameter, String newName) {
    return ParameterSpec.builder(parameter.type, newName)
        .addAnnotations(parameter.annotations)
        .addModifiers(parameter.modifiers)
        .build();
  }

  /** Builds a root component implementation. */
  static final class RootComponentImplementationBuilder extends ComponentImplementationBuilder {
    @Inject
    RootComponentImplementationBuilder(ComponentImplementation componentImplementation) {
      checkArgument(!componentImplementation.superclassImplementation().isPresent());
    }

    @Override
    protected void addCreatorClass(TypeSpec creator) {
      componentImplementation.addType(COMPONENT_CREATOR, creator);
    }

    @Override
    protected void addFactoryMethods() {
      // Top-level components have a static method that returns a builder or factory for the
      // component. If the user defined a @Component.Builder or @Component.Factory, an
      // implementation of their type is returned. Otherwise, an autogenerated Builder type is
      // returned.
      // TODO(cgdecker): Replace this abomination with a small class?
      // Better yet, change things so that an autogenerated builder type has a descriptor of sorts
      // just like a user-defined creator type.
      ComponentCreatorKind creatorKind;
      ClassName creatorType;
      String factoryMethodName;
      boolean noArgFactoryMethod;
      if (creatorDescriptor().isPresent()) {
        ComponentCreatorDescriptor descriptor = creatorDescriptor().get();
        creatorKind = descriptor.kind();
        creatorType = ClassName.get(descriptor.typeElement());
        factoryMethodName = descriptor.factoryMethod().getSimpleName().toString();
        noArgFactoryMethod = descriptor.factoryParameters().isEmpty();
      } else {
        creatorKind = BUILDER;
        creatorType = componentCreatorName();
        factoryMethodName = "build";
        noArgFactoryMethod = true;
      }

      MethodSpec creatorFactoryMethod =
          methodBuilder(creatorKind.methodName())
              .addModifiers(PUBLIC, STATIC)
              .returns(creatorType)
              .addStatement("return new $T()", componentCreatorName())
              .build();
      componentImplementation.addMethod(BUILDER_METHOD, creatorFactoryMethod);
      if (noArgFactoryMethod && canInstantiateAllRequirements()) {
        componentImplementation.addMethod(
            BUILDER_METHOD,
            methodBuilder("create")
                .returns(ClassName.get(super.graph.componentTypeElement()))
                .addModifiers(PUBLIC, STATIC)
                .addStatement(
                    "return new $L().$L()", creatorKind.typeName(), factoryMethodName)
                .build());
      }
    }

    private Optional<ComponentCreatorDescriptor> creatorDescriptor() {
      return graph.componentDescriptor().creatorDescriptor();
    }

    /** {@code true} if all of the graph's required dependencies can be automatically constructed */
    private boolean canInstantiateAllRequirements() {
      return !Iterables.any(
          graph.componentRequirements(),
          dependency -> dependency.requiresAPassedInstance(elements, types));
    }

    private ClassName componentCreatorName() {
      return componentImplementation.creatorImplementation().get().name();
    }
  }

  /**
   * Builds a subcomponent implementation. If generating ahead-of-time subcomponents, this may be an
   * abstract base class implementation, an abstract inner implementation, or a concrete
   * implementation that extends an abstract base implementation. Otherwise it represents a private,
   * inner, concrete, final implementation of a subcomponent which extends a user defined type.
   */
  static final class SubcomponentImplementationBuilder extends ComponentImplementationBuilder {
    final Optional<ComponentImplementationBuilder> parent;

    @Inject
    SubcomponentImplementationBuilder(
        @ParentComponent Optional<ComponentImplementationBuilder> parent) {
      this.parent = parent;
    }

    @Override
    protected void addCreatorClass(TypeSpec creator) {
      if (parent.isPresent()) {
        // In an inner implementation of a subcomponent the creator is a peer class.
        parent.get().componentImplementation.addType(SUBCOMPONENT, creator);
      } else {
        componentImplementation.addType(SUBCOMPONENT, creator);
      }
    }

    @Override
    protected void addFactoryMethods() {
      // Only construct instances of subcomponents that have concrete implementations.
      if (!componentImplementation.isAbstract()) {
        // Use the parent's factory method to create this subcomponent if the
        // subcomponent was not added via {@link dagger.Module#subcomponents()}.
        graph.factoryMethod().ifPresent(this::createSubcomponentFactoryMethod);
      }
    }

    private void createSubcomponentFactoryMethod(ExecutableElement factoryMethod) {
      checkState(parent.isPresent());

      Collection<ParameterSpec> params = getFactoryMethodParameters(graph).values();
      MethodSpec.Builder method = MethodSpec.overriding(factoryMethod, parentType(), types);
      params.forEach(
          param -> method.addStatement("$T.checkNotNull($N)", Preconditions.class, param));
      method.addStatement(
          "return new $T($L)", componentImplementation.name(), parameterNames(params));

      parent.get().componentImplementation.addMethod(COMPONENT_METHOD, method.build());
    }

    private DeclaredType parentType() {
      return asDeclared(parent.get().graph.componentTypeElement().asType());
    }

    @Override
    protected void addInterfaceMethods() {
      if (componentImplementation.superclassImplementation().isPresent()) {
        // Since we're overriding a subcomponent implementation we add to its implementation given
        // an expanded binding graph.

        ComponentImplementation superclassImplementation =
            componentImplementation.superclassImplementation().get();
        for (ModifiableBindingMethod superclassModifiableBindingMethod :
            superclassImplementation.getModifiableBindingMethods().values()) {
          bindingExpressions
              .modifiableBindingExpressions()
              .possiblyReimplementedMethod(superclassModifiableBindingMethod)
              .ifPresent(componentImplementation::addImplementedModifiableBindingMethod);
        }
      } else {
        super.addInterfaceMethods();
      }
    }

    @Override
    protected Optional<CodeBlock> cancelParentStatement() {
      if (!shouldPropagateCancellationToParent()) {
        return Optional.empty();
      }
      return Optional.of(
          CodeBlock.builder()
              .addStatement(
                  "$T.this.$N($N)",
                  parent.get().componentImplementation.name(),
                  CANCELLATION_LISTENER_METHOD_NAME,
                  MAY_INTERRUPT_IF_RUNNING)
              .build());
    }

    private boolean shouldPropagateCancellationToParent() {
      return parent.isPresent()
          && parent
              .get()
              .componentImplementation
              .componentDescriptor()
              .cancellationPolicy()
              .map(policy -> policy.fromSubcomponents().equals(PROPAGATE))
              .orElse(false);
    }
  }

  /**
   * Returns the map of {@link ComponentRequirement}s to {@link ParameterSpec}s for the
   * given graph's factory method.
   */
  private static Map<ComponentRequirement, ParameterSpec> getFactoryMethodParameters(
      BindingGraph graph) {
    return Maps.transformValues(graph.factoryMethodParameters(), ParameterSpec::get);
  }
}
