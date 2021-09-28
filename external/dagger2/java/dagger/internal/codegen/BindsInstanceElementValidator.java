/*
 * Copyright (C) 2016 The Dagger Authors.
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

import dagger.BindsInstance;
import javax.lang.model.element.Element;

abstract class BindsInstanceElementValidator<E extends Element> extends BindingElementValidator<E> {
  BindsInstanceElementValidator() {
    super(BindsInstance.class, AllowsMultibindings.NO_MULTIBINDINGS, AllowsScoping.NO_SCOPING);
  }

  @Override
  protected final String bindingElements() {
    // Even though @BindsInstance may be placed on methods, the subject of errors is the
    // parameter
    return "@BindsInstance parameters";
  }

  @Override
  protected final String bindingElementTypeVerb() {
    return "be";
  }
}
